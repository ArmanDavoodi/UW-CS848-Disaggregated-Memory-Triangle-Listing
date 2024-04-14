#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <random>
#include <cstring>

#include <graph.h>

using namespace std;

unsigned long long buffer_size = 102400;
string filename = "twitter_combined.txt";
int papp = 0; // 0 LDG, 1 random, 2 sequential
int niterations = 0; // -1 until finished
int indeg_threshold = 0; // -1 store all input edges
bool get_results_of_the_first_partitioning = true, reduce_until_fit = true;

string get_papp() {
    switch (papp)
    {
    case 0:
        return "LDG";
    case 1:
        return "Random";
    case 2:
        return "Sequential";
    default:
        return "Undefined";
    }
}

void preprocessing(Graph& g, int& nodes_removed, int& edge_removed, unsigned long long& size_diff, int niterations = -1) {
    set<int> *marked = new set<int>;
    
    unsigned long long old_size = g.get_size_bytes();
    g.size = 0;
    bool finished = false;

    while (niterations != 0 && !finished) {
        --niterations;
        finished = true;
        if (g.indeg_thresh == 0) {
            for (int i = 0; i < g.num_nodes; ++i) {
                if (g.nodes[i].removed || g.nodes[i].nfollowers >= 2)
                    continue;
                
                vector<int> rem;
                for (auto out : g.nodes[i].follows) {
                    if (g.nodes[out].removed) {
                        rem.push_back(out);
                        --g.nodes[i].nfollows;
                        g.nodes[i].size = 0;
                    }
                }
                for (auto out : rem)
                    g.nodes[i].follows.erase(out);
                rem.clear();

                if (g.nodes[i].nfollows + g.nodes[i].nfollowers < 2) {
                    ++nodes_removed;
                    if (g.nodes[i].nfollows + g.nodes[i].nfollowers > 0) {
                        finished = false;
                    }
                    g.nodes[i].removed = true;
                    g.nodes[i].size = 0;
                    for (auto out : g.nodes[i].follows) {
                        --g.nodes[out].nfollowers;
                        ++edge_removed;
                    }
                    edge_removed += g.nodes[i].nfollowers;
                }

            }
        }
        else {
            if (marked->empty()) {
                for (int i = 0; i < g.num_nodes; ++i) {
                    if (!g.nodes[i].removed && g.nodes[i].nfollows + g.nodes[i].nfollowers < 2) {
                        ++nodes_removed;
                        if (g.nodes[i].nfollows + g.nodes[i].nfollowers > 0)
                            finished = false;
                        g.nodes[i].removed = true;
                        g.nodes[i].size = 0;
                        for (auto out : g.nodes[i].follows) {
                            if (g.nodes[out].removed)
                                continue;
                            --g.nodes[out].nfollowers;
                            if (!g.nodes[out].followers.empty()) {
                                g.nodes[out].followers.erase(i);
                                g.nodes[out].size = 0;
                            }
                            ++edge_removed;

                            marked->insert(out);
                        }
                        
                        edge_removed += g.nodes[i].nfollowers;
                        for (auto in : g.nodes[i].followers) {
                            if (g.nodes[in].removed)
                                continue;
                            --g.nodes[in].nfollows;
                            g.nodes[in].follows.erase(i);
                            g.nodes[in].size = 0;
                            marked->insert(in);
                        }
                    }
                }
            }
            else {
                set<int> *tmarked = new set<int>;
                for (auto i : *marked) {
                    if (!g.nodes[i].removed && g.nodes[i].nfollows + g.nodes[i].nfollowers < 2) {
                        ++nodes_removed;

                        g.nodes[i].removed = true;
                        g.nodes[i].size = 0;
                        for (auto out : g.nodes[i].follows) {
                            if (g.nodes[out].removed)
                                continue;
                            --g.nodes[out].nfollowers;
                            if (!g.nodes[out].followers.empty()) {
                                g.nodes[out].followers.erase(i);
                                g.nodes[out].size = 0;
                            }
                            ++edge_removed;

                            tmarked->insert(out);
                        }

                        for (auto in : g.nodes[i].followers) {
                            if (g.nodes[in].removed)
                                continue;
                            --g.nodes[in].nfollows;
                            g.nodes[in].follows.erase(i);
                            g.nodes[in].size = 0;
                            ++edge_removed;
                            tmarked->insert(in);
                        }
                    }
                }
                delete marked;
                marked = tmarked;
                finished = marked->empty();
            }
        }
    }
    delete marked;

    size_diff = old_size - g.get_size_bytes();
}

struct Partition {
    vector<int> inodes;
    set<int> edge_nodes;
    map<int, set<int>> incoming_edges;

    Graph& g;
    unsigned long long size = 0;
    int index = 0;

    Partition(int id, Graph& graph) : index(id), g(graph) {}

    unsigned long long new_size_if_added(int id) {
        int nsize = size;

       
        nsize += g.nodes[id].get_size_bytes();

        return nsize;
    }

    void add_node(int id) {
        inodes.push_back(id);
        g.nodes[id].partition = index;
        size += g.nodes[id].get_size_bytes();
    }

    unsigned long long get_extended_size() {
        int nsize = size;
        for (auto e : incoming_edges) {
            nsize += e.second.size() * sizeof(int);
        }
        return nsize;
    }
};

class PSet {
public:
    Graph& g;
    Graph* ng;
    NodeIdWrapper<int>* niw;
    bool ng_read = false;
    int npart = 2;
    int niter;
    int nodes_removed = 0;
    int edge_removed = 0;
    unsigned long long size_diff = 0;
    vector<Partition> partitions;


    PSet(Graph &graph, int num_partitions = 2, int niterations = -1, int indeg_thresh = 0) : g(graph), npart(num_partitions), niter(niterations) {
        npart = std::max(2, npart);
        ng = new Graph(indeg_thresh);
        niw = new NodeIdWrapper<int>;
        for (int i = 0; i < npart; ++i) {
            partitions.push_back(Partition(i, g));
        }

        preprocessing(g, nodes_removed, edge_removed, size_diff, niterations);
    }

    ~PSet() {
        if (!ng_read) {
            delete ng;
        }
        delete niw;
    }

    void optimal_par() {
        vector<int> part_edge[g.num_nodes];
        vector<set<int>> in_edges(g.num_nodes);
        unsigned long long rsize = g.get_size_bytes();

        int tpart = max(npart, (int)(g.get_size_bytes() / buffer_size) + ((g.get_size_bytes() % buffer_size) > 0));
        if (tpart > npart) {
            for (; npart < tpart; ++npart) {
                partitions.push_back(Partition(npart, g));
            }
        }

        for (int i = 0; i < g.num_nodes; ++i) {
            if (g.nodes[i].removed)
                continue;

            for (int j = 0; j < npart; ++j) {
                part_edge[i].push_back(0);
            }
        }

        for (int i = 0; i < g.num_nodes; ++i) {
            if (g.nodes[i].removed)
                continue;

            for (auto on : g.nodes[i].follows) {
                if (g.nodes[on].removed)
                    continue;

                if (g.nodes[on].partition != -1) {
                    ++part_edge[i][g.nodes[on].partition];
                }
            }

            double maxp = -1;
            for (int j = 0; j < npart; ++j) {
                unsigned long long ns = partitions[j].new_size_if_added(i);
                double cmaxp = part_edge[i][j] * (1.0 - (double)ns/(double)buffer_size);
                if (cmaxp > maxp && buffer_size >= ns) {
                    g.nodes[i].partition = j;
                    maxp = cmaxp;
                }
            }

            if (maxp < 0) {
                int t = npart;
                tpart += (int)(rsize / buffer_size) + ((rsize % buffer_size) > 0);
                for (; npart < tpart; ++npart) {
                    partitions.push_back(Partition(npart, g));
                    for (int k = 0; k < g.num_nodes; ++k) {
                        if (g.nodes[k].removed)
                            continue;
                            part_edge[k].push_back(0);
                    }
                }

                partitions[t].add_node(i);

            }
            else {
                partitions[g.nodes[i].partition].add_node(i);
            }
            rsize -= g.nodes[i].get_size_bytes();

            for (auto on : g.nodes[i].follows) {
                if (g.nodes[on].removed)
                    continue;

                ++part_edge[on][g.nodes[i].partition];
                if (g.nodes[i].partition != g.nodes[on].partition) {
                    if (g.nodes[on].partition != -1) {
                        add_edge(*ng, *niw, i, on);
                        partitions[g.nodes[on].partition].incoming_edges[on].insert(i);
                    }
                    else {
                        in_edges[on].insert(i);
                    }
                }
            }

            for (int incoming : in_edges[i]) {
                if (g.nodes[incoming].partition != g.nodes[i].partition) {
                    add_edge(*ng, *niw, incoming, i);
                    partitions[g.nodes[i].partition].incoming_edges[i].insert(incoming);
                }
            }
            in_edges[i].clear();
        }
    }

    void random_par() {
        vector<set<int>> in_edges(g.num_nodes);
        set<int> remaining;
        unsigned long long rsize = g.get_size_bytes();

        for (int i = 0; i < npart; ++i) {
            remaining.insert(i);
        }

        int tpart = max(npart, (int)(g.get_size_bytes() / buffer_size) + ((g.get_size_bytes() % buffer_size) > 0));
        if (tpart > npart) {
            for (; npart < tpart; ++npart) {
                partitions.push_back(Partition(npart, g));
                remaining.insert(npart);
            }
        }
        
        default_random_engine generator((unsigned int)time(0));
        uniform_int_distribution<int> *distribution = new uniform_int_distribution<int>(0, remaining.size() - 1);
        

        for (int i = 0; i < g.num_nodes; ++i) {
            if (g.nodes[i].removed)
                continue;

            int j;
            unsigned long long ns;
            do {
                auto t = remaining.begin();
                for (j = (*distribution)(generator); j > 0; --j, ++t) {}
                j = *(t);

                ns = partitions[j].new_size_if_added(i);
                if (buffer_size < ns) {
                    remaining.erase(t);
                    if (remaining.empty()) {
                        tpart += (int)(rsize / buffer_size) + ((rsize % buffer_size) > 0);
                        for (; npart < tpart; ++npart) {
                            partitions.push_back(Partition(npart, g));
                            remaining.insert(npart);
                        }
                    }

                    delete distribution;
                    distribution = new uniform_int_distribution<int>(0, remaining.size() - 1);
                }

            } while (buffer_size < ns);

            partitions[j].add_node(i);
            rsize -= g.nodes[i].get_size_bytes();

            for(auto on : g.nodes[i].follows) {
                if (g.nodes[on].removed)
                    continue;


                if (g.nodes[i].partition != g.nodes[on].partition) {
                    if (g.nodes[on].partition != -1) {
                        add_edge(*ng, *niw, i, on);
                        partitions[g.nodes[on].partition].incoming_edges[on].insert(i);
                    }
                    else {
                        in_edges[on].insert(i);
                    }
                }
            }

            for (int incoming : in_edges[i]) {
                if (g.nodes[incoming].partition != g.nodes[i].partition) {
                    add_edge(*ng, *niw, incoming, i);
                    partitions[g.nodes[i].partition].incoming_edges[i].insert(incoming);
                }
            }
            in_edges[i].clear();
        }

        delete distribution;
    }

    void sequential_par() {
        vector<set<int>> in_edges(g.num_nodes);
        vector<bool> seen(g.num_nodes, false);
        vector<pair<int, int>> dfs_stack;
        int max_depth = 3, depth = 0;


        int tpart = max(npart, (int)(g.get_size_bytes() / buffer_size) + ((g.get_size_bytes() % buffer_size) > 0));
        if (tpart > npart) {
            for (; npart < tpart; ++npart) {
                partitions.push_back(Partition(npart, g));
            }
        }
        int current_partition = 0;        

        for (int i = 0; i < g.num_nodes; ++i) {
            if (g.nodes[i].removed || seen[i])
                continue;
            
            depth = 0;
            dfs_stack.push_back(make_pair(i, depth));
            seen[i] = true;
            while (!dfs_stack.empty()) {
                int node = dfs_stack.back().first;                
                depth = dfs_stack.back().second;
                dfs_stack.pop_back();

                if (buffer_size < partitions[current_partition].new_size_if_added(node)) {
                    ++current_partition;
                    if (current_partition == npart) {
                        partitions.push_back(Partition(npart, g));
                        ++npart;
                    }
                }

                partitions[current_partition].add_node(node);


                for (int child : g.nodes[node].follows) {
                    if (g.nodes[child].removed)
                        continue;

                    if (g.nodes[node].partition != g.nodes[child].partition) {
                        if (g.nodes[child].partition != -1) {
                            add_edge(*ng, *niw, node, child);
                            partitions[g.nodes[child].partition].incoming_edges[child].insert(node);
                        }
                        else {
                            in_edges[child].insert(node);
                        }
                    }

                    if (!seen[child] && depth < 3) {
                        dfs_stack.push_back(make_pair(child, depth+1));
                        seen[child] = true;
                    }
                }

                for (int incoming : in_edges[node]) {
                    if (g.nodes[incoming].partition != g.nodes[node].partition) {
                        add_edge(*ng, *niw, incoming, node);
                        partitions[g.nodes[node].partition].incoming_edges[node].insert(incoming);
                    }
                }
                in_edges[i].clear();
            }
        }

    }

    Graph* get_reduced_graph() {
        ng_read = true;
        return ng;
    }

};

/*
3 files
1) graphs
id*, file_name*, num_nodes*, num_edges*, graph_size*, algorithm, buffersize, w/o preprocessing, num_reductions
2) each reduction
id*, num_nodes, num_edges, size, num_partitions, edge_cut_ratio
*/
void get_results(Graph& g, PSet& p) {
    int npp[p.npart];
    int **bpe = new int*[p.npart];
    int num_e = 0;
    int bpen = 0;
    set<int> bpnodes;
    unsigned long long bpgsize = 0;
    int nodes_without_part = 0;
    unsigned long long nwpsize = 0;
   

    for (int i = 0; i < p.npart; ++i) {
        npp[i] = 0;
        bpe[i] = new int[p.npart];
        // spp[i] = 0;
        for (int j = 0; j < p.npart; ++j) {
            bpe[i][j] = 0;
        }
    }

    for (int i = 0; i < g.num_nodes; ++i) {
        if (g.nodes[i].removed)
            continue;
        for (auto on : g.nodes[i].follows) {
            if (g.nodes[on].removed)
                continue;
            ++bpe[g.nodes[i].partition][g.nodes[on].partition];
            if (g.nodes[i].partition != g.nodes[on].partition) {
                ++bpen;
                bpnodes.insert(i);
                bpnodes.insert(on);
            }
            ++num_e;
        }
        ++npp[g.nodes[i].partition];
        if (g.nodes[i].partition == -1) {
            nodes_without_part += 1;
            nwpsize += g.nodes[i].get_size_bytes();
        }

    }

    for (auto i : bpnodes) {
        bpgsize += g.nodes[i].get_size_bytes();
    }

    cout << "# of partitions = " << p.npart << endl;
    cout << "# of nodes without partition = " << nodes_without_part << ", size nodes without partitions = " << nwpsize << endl;
    cout << "# of edge partition nodes = " << bpnodes.size() << ", # of between partitions edges = " << bpen << ", # of between partitions size = " << bpgsize << endl;
    cout << "partition, nodes, edges, size\n";
    for (int i = 0; i < p.npart; ++i) {
        cout << i << ", " << npp[i] << ", " << bpe[i][i] << ", " << p.partitions[i].size << endl;;
    }
    cout << "\n";
    cout << "partition, edges to other partitions " << endl;
    for (int i = 0; i < p.npart; ++i) {
        int ne = 0;
        for (int j = 0; j < p.npart; ++j) {
            ne += bpe[i][j];
        }
        cout << i << ", " << ne << endl;
    }
    cout << "\n";

    for (int i = 0; i < p.npart; ++i) {
        delete[] bpe[i];
    }
    delete[] bpe;
}

int main(int argc, char *argv[]) {
    // filedir [buffer_size | d [partitioning_algorithm | d [preprocessing_num_iterations | d [preprocessing_indegree_store_threshold [get_results_of_the_first_partitioning | d [reduce_until_fit | d]]]]]] 
    switch (argc) {
    case 8:
        if (strcmp(argv[7], "d")) {
            if (!strcmp(argv[7], "true") || !strcmp(argv[7], "True"))
                reduce_until_fit = true;
            else if (!strcmp(argv[7], "false") || !strcmp(argv[7], "False"))
                reduce_until_fit = false;
            else {
                cerr << "Error: undefined reduce_until_fit argument \"" << argv[7] << "\"!\n";
                cerr << "Defined arguments are: [True, true, False, false, d].\n";
                cerr << "Use --help for more information.\n";
            }
        }
    case 7:
        if (strcmp(argv[6], "d")) {
            if (!strcmp(argv[6], "true") || !strcmp(argv[6], "True"))
                get_results_of_the_first_partitioning = true;
            else if (!strcmp(argv[6], "false") || !strcmp(argv[6], "False"))
                get_results_of_the_first_partitioning = false;
            else {
                cerr << "Error: undefined get_results_of_the_first_partitioning argumet \"" << argv[6] << "\"!\n";
                cerr << "Defined arguments are: [True, true, False, false, d].\n";
                cerr << "Use --help for more information.\n";
            }
        }
    case 6:
        if (strcmp(argv[5], "d")) {
            indeg_threshold = stoi(argv[5]);
            if (indeg_threshold < 0) {
                indeg_threshold = -1;
            }
        }
    case 5:
        if (strcmp(argv[4], "d")) {
            niterations = stoi(argv[4]);
            if (niterations < 0) {
                niterations = -1;
            }
        }
        else {
            niterations = -1;
        }
    case 4:
        if (strcmp(argv[3], "d")) {
            if (!strcmp(argv[3], "LDG") || !strcmp(argv[3], "ldg")) {
                papp = 0;
            }
            else if (!strcmp(argv[3], "Random") || !strcmp(argv[3], "random")) {
                papp = 1;
            }
            else if (!strcmp(argv[3], "Sequential") || !strcmp(argv[3], "sequential")) {
                papp = 2;
            }
            else {
                cerr << "Error: undefined algorithm \"" << argv[3] << "\"!\n";
                cerr << "Defined algorithm arguments are: [LDG, ldg, Random, random, Sequential, sequential, d].\n";
                cerr << "Use --help for more information.\n";
                exit(1);
            }
        }
    case 3:
        if (strcmp(argv[2], "d")) {
            buffer_size = stoi(argv[2]);
            if (buffer_size < 512) {
                cerr << "Error: buffer_size should be at least 512 bytes!\n";
                cerr << "Use --help for more information.\n";
                exit(1);
            }
        }
    case 2:
        if (!strcmp(argv[1], "--help")) {
            cout << "Usage: filedir [buffer_size | d [partitioning_algorithm | d [preprocessing_num_iterations | d [preprocessing_indegree_store_threshold [get_results_of_the_first_partitioning | d [reduce_until_fit | d]]]]]] \n";
            cout << "filedir: should be the address of the file containing list of all graph edges.\n";
            cout << "buffer_size: determines the size of the Compute Node's memory in bytes ( >= 512). It is set to 100 KB by default.\n";
            cout << "partitioning_algorithm: determins the partitioning algorithm to be used. Options are LDG(default), Random and Sequential.\n";
            cout << "preprocessing_num_iterations: determins the number of iterations done in preprocessing.\n" 
                << "\t* If not declared or set to 0, preprocessing is disabled.\n" 
                << "\t* If it is declared and set to default, or is less than 0, preprocessing is enabled and has no limit on number of iterations.\n"
                << "\t* Otherwise, preprocessing is enabled and iterates at most up to the specified number.\n";
            cout << "preprocessing_indegree_store_threshold: determins the threshold for having a copy of the incoming edges for each node.\n"
                << "\t* If not declared or set to 0(default), incoming edges are not stored per graph node.\n" 
                << "\t* If set to less than 0, Each node also stores a list of incomming edges.\n"
                << "\t* Otherwise, Only nodes with input-degree less than the specified amount, store a list of their incoming edges.\n";
            cout << "get_results_of_the_first_partitioning: Can be set true(default) or false. If set true, it will print information about first partitioning phase.\n";
            cout << "reduce_until_fit: Can be set true(default) or false. If set true, it will repeat the algorithm until the reduced graph fits inside the buffer.\n";
            cout << "You can use --help to view this message again.\n";

            return 0;
        }
        else {
            filename = argv[1];
        }

        break;
    
    default:
        cerr << "Usage Error: filedir [buffer_size | d [partitioning_algorithm | d [preprocessing_num_iterations | d [preprocessing_indegree_store_threshold [get_results_of_the_first_partitioning | d [reduce_until_fit | d]]]]]] \n";
        cerr << "Use --help for more information.\n";
        exit(1);
    }
    
    cout << "reading from " << filename << ", partitioning approach: " << get_papp() << ", buffer_size = " << buffer_size << endl;
    cout << "preprocessing: ";
    if (niterations == 0) {
        cout << "off" << endl;
    } 
    else {
        cout << "# of iterations " << (niterations < 0 ? "no limit" : to_string(niterations) ) 
            << ", indegree threshold: " << (indeg_threshold < 0 ? "no limit" : to_string(indeg_threshold) ) << endl;
    }
    cout << "##############################################################################\n";


    cout << "opening the file ...\n";
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Could not open the file.\n";
        cerr << "Use --help for more information.\n";
    }

    cout << "file opened.\n";
    NodeIdWrapper<string> idWrapper;
    Graph g(indeg_threshold);

    cout << "reading the edge list ...\n";
    while (file) {
        string id1, id2;
        file >> id1 >> id2;
        add_edge(g, idWrapper, id1, id2);
    }
    cout << "closing ...\n";
    file.close();
    cout << "file closed.\n";

    cout << "# of nodes = " << g.num_nodes << ", # of edges = " << g.num_edges << ", graph size = " << g.get_size_bytes() << " bytes" << endl;
    cout << "buffer size = " << buffer_size << endl;

    if (niterations != 0)
        cout << "preprocessing ...\n";

    PSet partitions(g, 2, niterations, indeg_threshold);
    // if (niterations != 0) {
    //     cout << "preprocessing removed " << partitions.nodes_removed << " nodes and " << partitions.edge_removed << " edges.\n";
    // }

    cout << "partitioning ...\n";
    switch (papp)
    {
    case 0:
        partitions.optimal_par();
        break;
    case 1:
        partitions.random_par();
        break;
    case 2:
        partitions.sequential_par();
        break;
    default:
        break;
    }

    cout << "first partitioning is over.\n";
    Graph *reduced = partitions.get_reduced_graph();
    cout << "\nphase, ";
    if (niterations != 0) {
        cout << "preprocessing nodes removed, preprocessing edges removed, ";
    }
    cout << "cut-graph nodes, cut-graph edges, cut-graph size, # partitions\n1, ";
    if (niterations != 0) {
        cout << partitions.nodes_removed << ", " << partitions.edge_removed << ", ";
    }
    cout << reduced->num_nodes << ", " << reduced->num_edges << ", " 
        << reduced->get_size_bytes() << ", " << partitions.npart << "\n";

    if (reduce_until_fit) {
        int phases = 1;
        while(reduced->get_size_bytes() > buffer_size) {
            // cout << "continue to phase " << phases << " ...\n";
            ++phases;
            PSet *partitions = new PSet(*reduced, 2, niterations, indeg_threshold);
            // if (niterations != 0) {
            //     cout << "preprocessing removed " << partitions->nodes_removed << " nodes and " << partitions->edge_removed << " edges.\n";
            // }

            switch (papp)
            {
            case 0:
                partitions->optimal_par();
                break;
            case 1:
                partitions->random_par();
                break;
            case 2:
                partitions->sequential_par();
                break;
            default:
                break;
            }

            delete reduced;
            reduced = partitions->get_reduced_graph();
            cout << phases << ", ";
            if (niterations != 0) {
                cout << partitions->nodes_removed << ", " << partitions->edge_removed << ", ";
            }
            cout << reduced->num_nodes << ", " << reduced->num_edges << ", " 
                << reduced->get_size_bytes() << ", " << partitions->npart << "\n";
            
            delete partitions;
        }
        cout << "done in " << phases << " phases.\n";
    }
    delete reduced;


    if (get_results_of_the_first_partitioning) {
        cout << "preparing the results for the first partition.\n";
        
        get_results(g, partitions);
    }
}