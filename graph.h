#ifndef GRAPH_H
#define GRAPH_H

#include <set>
#include <map>
#include <vector>
#include <string>

template<typename T>
class NodeIdWrapper {
public:
    std::map<T, int> node_map;

    int get_id(T id) {
        auto it = node_map.find(id);
        if (it == node_map.end()) {
            return -1;
        }
        else 
            return it->second;
    }

    int add_id(T id, int iid) {
        node_map[id] = iid;
        return iid;
    }
};

class NodeAdjList {
public:
    NodeAdjList(int nid) : id(nid) {}

    unsigned long long get_size_bytes();

    int id;
    int nfollows = 0;
    int nfollowers = 0;
    int partition = -1;
    unsigned long long size = 0;
    bool removed = false;
    std::set<int> follows;
    std::set<int> followers;
};

class Graph {
public:
    std::vector<NodeAdjList> nodes;
    int num_nodes = 0;
    unsigned long long num_edges = 0;
    unsigned long long size = 0;
    int indeg_thresh = 0; // -1 -> store all incomming neighbors, n -> store up to n incomming neighbors

    Graph(int it) : indeg_thresh(it) {}

    unsigned long long get_size_bytes() {
        if (size == 0) {
            for (int i = 0; i < num_nodes; ++i) {
                // std::cout << i << std::endl;
                if (!nodes[i].removed)
                    size += nodes[i].get_size_bytes();
            }
        }
        return size;   
    }

    int add_node() {
        nodes.push_back(NodeAdjList(num_nodes));
        ++num_nodes;
        return num_nodes - 1;
    }

    bool add_edge(int id1, int id2) {
        if (id1 >= 0 && id2 >= 0 && id1 < num_nodes && id2 < num_nodes && nodes[id1].follows.find(id2) == nodes[id1].follows.end()) {
            nodes[id1].follows.insert(id2);
            size = 0;
            nodes[id1].size = 0;

            if (indeg_thresh < 0 || nodes[id2].nfollowers < indeg_thresh) {
                nodes[id2].followers.insert(id1);
                nodes[id2].size = 0;
            }
            else if (nodes[id2].nfollowers == indeg_thresh) {
                nodes[id2].followers.clear();
                nodes[id2].size = 0;
            }

            ++nodes[id1].nfollows;
            ++nodes[id2].nfollowers;
            ++num_edges;
            return true;
        }
        return false;
    }
};

unsigned long long NodeAdjList::get_size_bytes() {
    if (size == 0) {
        size = sizeof(NodeAdjList) - 2 * sizeof(int) + sizeof(int) * (follows.size() + followers.size());
    }
    return size;
}

template <typename T>
void add_edge(Graph& g, NodeIdWrapper<T>& idWrapper, T& id1, T& id2) {
    int i1 = idWrapper.get_id(id1);
    if (i1 < 0) {
        i1 = idWrapper.add_id(id1, g.add_node());
    }

    int i2 = idWrapper.get_id(id2);
    if (i2 < 0) {
        i2 = idWrapper.add_id(id2, g.add_node());
    }

    g.add_edge(i1, i2);
}

#endif