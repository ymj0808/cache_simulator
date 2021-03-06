#include "cluster_variants.h"
#include "../random_helper.h"
//#include "gd_variants.h"
#include <algorithm>
#include <cassert>
#include <cassert>
#include <cmath>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <queue>
#include <string.h>
#include <unordered_map>
#include <vector>


using namespace std;

/*
    Consistent hash
*/

bool CHCache::lookup(SimpleRequest* req) {
    auto cache_index = chash.look_up(std::to_string(req->getId())).second;
    return caches_list[cache_index].lookup(req);
}

void CHCache::admit(SimpleRequest* req) {
    auto cache_index = chash.look_up(std::to_string(req->getId())).second;
    caches_list[cache_index].admit(req);
}

void CHCache::setPar(std::string parName, std::string parValue) {
    if (parName.compare("n") == 0) {
        const int n = stoull(parValue);
        cache_number = n;
        caches_list = new LRUCache[cache_number];
        assert(n > 1);
        for (int i = 0; i < n; ++i) {
            caches_list[i].setSize(_cacheSize);
        }
    }
    else if (parName.compare("vnode") == 0) {
        const int param = stoull(parValue);
        virtual_node = param;
    }
    else if (parName.compare("map") == 0) {
        const int map = stoull(parValue);
        printMap = map;
    }
    else {
        std::cerr << "unrecognized parameter: " << parName << std::endl;
    }
}

void CHCache::init_mapper() {
    int ip_seg_3 = 0;
    int ip_seg_4 = 136;
    string ip = "";    
    for (int rnode_num = 0; rnode_num < cache_number; rnode_num++) {
        ip =
            "192.168.0" + std::to_string(ip_seg_3) + "." + std::to_string(ip_seg_4);
        chash.add_real_node(ip, virtual_node);
        ip_seg_3++;
        ip_seg_4++;
    }
    
    // CpuMonitor cpu_mon = CpuMonitor(); // Peixuan 01182021
}

bool CHCache::request(SimpleRequest* req) {
    auto cache_index = chash.look_up(std::to_string(req->getId())).second;
    // auto cache_index = mapper.find(obj)->second; // redirect to small cache
    bool flag = caches_list[cache_index].lookup(req);
    if (!flag) {
        caches_list[cache_index].admit(req);
    }

    // 01182021 Peixuan
    position++;
    if (position == window_size) {
        position = 0;
        double percentage = cpu_mon.Get();
        cout << "CPU usage: " << percentage << std::endl;
    }

    return flag;
}

void CHCache::printReqAndFileNum() {
    cout << "request number: ";
    for (int i = 0; i < cache_number; i++) {
        cout << caches_list[i].requestNum() << "  ";
    }
    cout << endl << "unique file number: ";
    for (int i = 0; i < cache_number; i++) {
        cout << caches_list[i].uniqueFileNum() << "  ";
    }
    cout << endl;

    if (printMap) {
        // 10262020 Peixuan: print file mapping

        std::map<std::string, unsigned int>::iterator iter;

        std::map<std::string, unsigned int> fileID_vnode_map =
            chash.fileID_vnode_map;
        iter = fileID_vnode_map.begin();
        while (iter != fileID_vnode_map.end()) {
            cout << iter->first << " :[vnode] " << iter->second << endl;
            iter++;
        }

        std::map<std::string, unsigned int> fileID_rnode_map =
            chash.fileID_rnode_map;
        iter = fileID_rnode_map.begin();
        while (iter != fileID_rnode_map.end()) {
            cout << iter->first << " :[rnode] " << iter->second << endl;
            iter++;
        }
    }
}

/*
    Adaptive Consistent hash
   ***************************************************************************************
*/

bool ACHCache::lookup(SimpleRequest* req) {
    auto cache_index = chash.look_up(std::to_string(req->getId())).second;
    return caches_list[cache_index].lookup(req);
}

void ACHCache::admit(SimpleRequest* req) {
    auto cache_index = chash.look_up(std::to_string(req->getId())).second;
    caches_list[cache_index].admit(req);
}

void ACHCache::setPar(std::string parName, std::string parValue) {
    if (parName.compare("n") == 0) { // set the number of servers
        const int n = stoull(parValue);
        cache_number = n;
        caches_list = new LRUCache[cache_number];
        // caches_list = new FilterCache[cache_number];
        assert(n > 1);
        for (int i = 0; i < n; ++i) {
            caches_list[i].setSize(_cacheSize);
            caches_list[i].setPar("n", "1");
        }
    }
    else if (parName.compare("W") == 0) { // set the window size
        const int w = stoull(parValue);
        window_size = w;
    }
    else if (parName.compare("alpha") == 0) {
        double param = stoull(parValue);
        while (param > 1.0)
            param /= 10;
        alpha = param;
    }
    else if (parName.compare("vnode") == 0) {
        const int param = stoull(parValue);
        virtual_node = param;
    }
    else if (parName.compare("t") == 0) {
        const int param = stoull(parValue);
        threshold = param;
    }
    else {
        std::cerr << "unrecognized parameter: " << parName << std::endl;
    }
}

void ACHCache::print_hash_space() {
    for (int i = 0; i < 4; i++) {
        std::cout << vnode_index_for_each_real_node[i].size() << ',';
    }
    std::cout << std::endl;
}

void ACHCache::init_mapper() {
    for (int i = 0; i < cache_number; ++i) {
        chash.add_real_node(std::to_string(i), virtual_node);
    }

    request_count = new uint64_t[cache_number];
    hit_count = new uint64_t[cache_number];
    miss_count = new uint64_t[cache_number];
    miss_rate = new double[cache_number];
    usage_ratio = new double[cache_number];
    rank = new double[cache_number];
    vnode_index_for_each_real_node.resize(cache_number);

    for (int i = 0; i < cache_number; ++i) {
        request_count[i] = 0;
        hit_count[i] = 0;
        miss_count[i] = 0;
        miss_rate[i] = 0.0;
        usage_ratio[i] = 0.0;
    }

    time = std::chrono::steady_clock::now();
    std::cerr << "init done" << std::endl;
    virtual_node_number = chash.sorted_node_hash_list.size();

    frag_arrs.resize(virtual_node_number, { {0, 0} });
    frag_arrs_rnode.resize(cache_number, { {0, 0} });

    head = new dequeue_node(nullptr);
    pointer = head;
    for (uint32_t j = 2; j <= window_size; ++j) {
        pointer->next = new dequeue_node(pointer);
        pointer = pointer->next;
    }
    tail = pointer;
    pointer = head;

    last_access_on_each_virtual_node.resize(virtual_node_number, {});
    last_access_on_each_real_node.resize(cache_number, {});
    for (int vnode = 0; vnode < virtual_node_number; ++vnode) {
        int cache_index =
            chash.virtual_node_map[chash.sorted_node_hash_list[vnode]].cache_index;
        cache_index_each_node.push_back(cache_index);
        vnode_index_for_each_real_node[cache_index].push_back(vnode);
    }

    std::cout << virtual_node_number << std::endl;
    std::cout << cache_index_each_node.size() << std::endl;
}

void ACHCache::reset() {
    position = 0;
    pointer = head;

    for (int i = 0; i < cache_number; ++i) {
        request_count[i] /= 2; // moving average
        hit_count[i] = 0;
        miss_count[i] = 0;
        usage_ratio[i] = 0;
        rank[i] = 0.0;
    }

    for (int vnode = 0; vnode < virtual_node_number; ++vnode) {
        frag_arrs[vnode].clear();
        frag_arrs[vnode][0] = 0;
        last_access_on_each_virtual_node[vnode].clear();
    }

    for (int rnode = 0; rnode < cache_number; ++rnode) {
        frag_arrs_rnode[rnode].clear();
        frag_arrs_rnode[rnode][0] = 0;
        last_access_on_each_real_node[rnode].clear();
    }

    while (pointer) { // clean arr
        pointer->arr.clear();
        pointer->arr_rnode.clear();
        pointer = pointer->next;
    }

    pointer = head;
}

void ACHCache::update() {
    max_requests = request_count[0];
    for (int i = 1; i < cache_number;
        ++i) { //  find the rnode with highest request number
        if (max_requests < request_count[i]) {
            max_requests = request_count[i];
        }
    }
    for (int i = 0; i < cache_number; ++i) {
        miss_rate[i] = (double)miss_count[i] / (miss_count[i] + hit_count[i]);
        // std::cout << miss_count[i] << '\t';
        // std::cout << request_count[i] << '\t';
        // std::cout << miss_rate[i] << '\t';
        usage_ratio[i] = (double)request_count[i] / max_requests;
        // std::cout<<usage_ratio[i]<<'\t';
        rank[i] = miss_rate[i] * alpha +
            usage_ratio[i] * (1 - alpha); // Peixuan read: get the rank by
                                          // combining missing rate and usage
                                          // ratio
                                          // std::cout << rank[i] << '*';
    }
    max_i = 0;
    min_i = 0;
    max_rank = rank[0];
    min_rank = rank[0];

    for (int i = 1; i < cache_number;
        ++i) { // Peixuan read: Find max and min rank rnode
        if (rank[i] > max_rank) {
            max_rank = rank[i];
            max_i = i;
        }
        if (rank[i] < min_rank) {
            min_rank = rank[i];
            min_i = i;
        }
    }

    SD_Max = 0;
    target = vnode_index_for_each_real_node[max_i].end(); // init target as NAN
    pointer = head;
    position = 0;
    while (pointer != nullptr) {
        if (pointer->real_node == max_i || pointer->real_node == min_i) {
            // the fragment array (stored in std::set) of this rnode
            auto& frag_arr_rnode = frag_arrs_rnode[pointer->real_node];
            auto size = pointer->content_size;
            if (pointer->last_access != UINT32_MAX) {
                auto start = frag_arr_rnode.find(pointer->last_access);
                auto next = ++start;
                while (++start != frag_arr_rnode.end()) // next never be the end()
                    start->second += size;
                frag_arr_rnode.erase(next);
            }
            else {
                for (auto& ele : frag_arr_rnode)
                    ele.second += size;
            }
            frag_arr_rnode[position + 1] = 0;
            pointer->copy_arr_rnode(
                frag_arr_rnode); // Peixuan: Copy rnode requedt to pointer arr
            if (pointer->last_access != UINT32_MAX) {
                pointer->c = pointer->c_value(pointer->last_access);
                pointer->c_vnode = pointer->c_value_vnode(pointer->last_access);
                if (pointer->c < threshold)
                    SD_Max++;
            }
        }
        pointer = pointer->next;
        ++position;
    }
    std::cout << "SD_MAX : " << SD_Max
        << "  "; // Peixuan Q: current SD_Max is the SD before moving?
    pointer = tail;
    for (auto iter = vnode_index_for_each_real_node[max_i].begin();
        iter != vnode_index_for_each_real_node[max_i].end();
        iter++) { // try to give vnode from CMax to CMin
        int SD = 0;
        while (pointer != nullptr) {
            if (pointer->real_node == min_i) {
                if (pointer->last_access != UINT32_MAX)
                    queue_of_min.push(pointer);   // Peixuan: record min_rnode request
                while (!queue_of_c_i.empty()) { // all c_i after pointer == min_i, //
                                                // Peixuan: c_i is request on the
                                                // current moving node
                    dequeue_node* col = queue_of_c_i.front();
                    // Peixuan: vnode c_i sd + rnode min sd and find if hit
                    if (col->c_vnode + pointer->c_value(col->last_access) < threshold) {
                        SD++;
                    }
                    queue_of_c_i.pop();
                }
            }
            else if (pointer->virtual_node ==
                *iter) { // Peixuan: iter is the current moving vnode
                if (pointer->last_access != UINT32_MAX)
                    queue_of_c_i.push(
                        pointer); // Peixuan: record c_i_vnode (iter) request
                while (!queue_of_max.empty()) { // all max after iter == c_i
                    dequeue_node* col = queue_of_max.front();
                    if (col->c - pointer->c_value_vnode(col->last_access) <
                        threshold) { // Peixuan: max_sd - ci_sd < thresh see if hit
                        SD++;
                    }
                    queue_of_max.pop();
                }
                while (!queue_of_min.empty()) { // all min after iter == c_i
                    dequeue_node* col = queue_of_min.front();
                    if (col->c + pointer->c_value_vnode(col->last_access) <
                        threshold) { // Peixuan: min_sd + ci_sd < thresh see if hit
                        SD++;
                    }
                    queue_of_min.pop();
                }
            }
            else if (pointer->real_node == max_i &&
                pointer->last_access != UINT32_MAX) {
                queue_of_max.push(pointer); // Peixuan: record max_rnode request
                                            // (request on max except the ones on
                                            // c_i(iter vnode))
            }
            pointer = pointer->prev;
        }
        while (!queue_of_c_i.empty()) {
            if (queue_of_c_i.front()->c_vnode < threshold)
                SD++;
            queue_of_c_i.pop();
        }
        while (!queue_of_max.empty()) {
            if (queue_of_max.front()->c < threshold)
                SD++;
            queue_of_max.pop();
        }
        while (!queue_of_min.empty()) {
            if (queue_of_min.front()->c < threshold)
                SD++;
            queue_of_min.pop();
        }
        if (SD > SD_Max) {
            SD_Max = SD;
            target = iter;
        }
        pointer = tail;
    }
    std::cout << "SD_MAX : " << SD_Max << std::endl;
    // change the cache_index attribute of virtual node
    if (target != vnode_index_for_each_real_node[max_i].end()) {
        std::cout << "Change " << max_i << " to " << min_i << std::endl;
        chash.virtual_node_map[chash.sorted_node_hash_list[*target]].cache_index =
            min_i;
        // change the vnode_index_for_each_real_node, put the virtual node from
        // max_i list to min_i list
        vnode_index_for_each_real_node[min_i].push_back(*target);
        vnode_index_for_each_real_node[max_i].erase(target);
    }
    reset();
}

bool ACHCache::request(SimpleRequest* req) {
    look_up_res = chash.look_up(
        std::to_string(req->getId())); // <virtual node index, real node index>
    request_count[look_up_res.second]++;
    auto size = req->getSize();

    iter_in_last_access =
        last_access_on_each_virtual_node[look_up_res.first].find(
            req->getId()); // <ID, last access> // Peixuan: find last access of
                           // this file ID on vnode
    // the fragment array (stored in std::set) of this vnode
    auto& frag_arr_vnode = frag_arrs[look_up_res.first];
    if (iter_in_last_access !=
        last_access_on_each_virtual_node[look_up_res.first].end()) {
        // accessed before
        auto start = frag_arr_vnode.find(iter_in_last_access->second);
        auto next = ++start;                    // the fragment need to be erased
        while (++start != frag_arr_vnode.end()) // next never be the end()
            start->second += size;                // those first time see this content
        frag_arr_vnode.erase(next);             // fragment merging, see the paper
        pointer->last_access = iter_in_last_access->second;
        iter_in_last_access->second = position;
    }
    else { // Peixuan: new file ID, first access
        for (auto& ele : frag_arr_vnode)
            ele.second += size;
        last_access_on_each_virtual_node[look_up_res.first][req->getId()] =
            position;
        pointer->last_access = UINT32_MAX;
    }
    frag_arr_vnode[position + 1] = 0; // the position for the first 0

    pointer->copy_arr(frag_arr_vnode, look_up_res,
        size); // Peixuan Q: copy frag_arr_vnode to arr?
// pointer->copy_arr(frag_arr_vnode, frag_arr_rnode,look_up_res);
    pointer->copy_arr_rnode(frag_arrs_rnode[look_up_res.second]); // even though
                                                                  // this do
                                                                  // nothing,
                                                                  // delete this
                                                                  // cause bug
    pointer = pointer->next;
    // std::cout << position << ",";
    position++;
    // Here is regular look up (in the cache server)
    flag = caches_list[look_up_res.second].lookup(req);
    if (!flag) {
        caches_list[look_up_res.second].admit(req);
        miss_count[look_up_res.second]++;
    }
    else {
        hit_count[look_up_res.second]++;
    }

    if (position == window_size) {
        update();
        // 01182021 Peixuan
        double percentage = cpu_mon.Get();
        cout << "CPU usage: " << percentage << std::endl;
    }
    return flag;
}

void ACHCache::printReqAndFileNum() {
    cout << "request number: ";
    for (int i = 0; i < cache_number; i++) {
        cout << caches_list[i].requestNum() << "  ";
    }
    cout << endl << "unique file number: ";
    for (int i = 0; i < cache_number; i++) {
        cout << caches_list[i].uniqueFileNum() << "  ";
    }
    cout << endl;
}