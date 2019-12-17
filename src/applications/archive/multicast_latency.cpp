#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "initialize.h"

#include <derecho/sst/multicast_sst.hpp>

using namespace std;
using namespace sst;

#ifndef NDEBUG
#define DEBUG_MSG(str)                 \
    do {                               \
        std::cout << str << std::endl; \
    } while(false)
#else
#define DEBUG_MSG(str) \
    do {               \
    } while(false)
#endif

volatile bool done = false;

int main(int argc, char* argv[]) {
    constexpr uint max_msg_size = 1;
    const unsigned int num_messages = 1000000;

    if(argc < 4) {
        cout << "Insufficient number of command line arguments" << endl;
        cout << "Usage: " << argv[0] << " <num_nodes> <window_size><num_senders_selector (0 - all senders, 1 - half senders, 2 - one sender)>" << endl;
        cout << "Thank you" << endl;
        exit(1);
    }
    uint32_t num_nodes = atoi(argv[1]);
    uint32_t window_size = atoi(argv[2]);
    int num_senders_selector = atoi(argv[3]);
    const uint32_t node_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
    const std::map<uint32_t, std::pair<ip_addr_t, uint16_t>> ip_addrs_and_ports = initialize(num_nodes);

    // initialize the rdma resources
#ifdef USE_VERBS_API
    verbs_initialize(ip_addrs_and_ports, node_id);
#else
    lf_initialize(ip_addrs_and_ports, node_id);
#endif

    std::vector<uint32_t> members;
    for(const auto& p : ip_addrs_and_ports) {
        members.push_back(p.first);
    }

    uint32_t num_senders = num_nodes, row_offset = 0;
    if(num_senders_selector == 0) {
    } else if(num_senders_selector == 1) {
        num_senders = num_nodes / 2;
        row_offset = (num_nodes + 1) / 2;
    } else {
        num_senders = 1;
        row_offset = num_nodes - 1;
    }

    std::shared_ptr<multicast_sst> sst = make_shared<multicast_sst>(
            sst::SSTParams(members, node_id),
            window_size,
            num_senders, max_msg_size);
    uint32_t node_rank = sst->get_local_index();

    // failures detection thread
    auto check_failures_loop = [&sst]() {
        pthread_setname_np(pthread_self(), "check_failures");
        while(true) {
            std::this_thread::sleep_for(chrono::microseconds(100));
            if(sst) {
                sst->put_with_completion((char*)std::addressof(sst->heartbeat[0]) - sst->getBaseAddress(), sizeof(bool));
            }
        }
    };
    thread failures_thread = std::thread(check_failures_loop);

    // data structures for the experiment
    vector<vector<struct timespec>> recv_times(num_nodes, vector<struct timespec>(num_messages, {0}));
    vector<struct timespec> send_times(num_messages, {0});

    uint64_t num_finished = 0;
    auto sst_receive_handler = [&](uint32_t sender_rank, uint64_t index, volatile char* msg, uint32_t size) {
        clock_gettime(CLOCK_REALTIME, &recv_times[sender_rank][index]);
        if(index == num_messages - 1) {
            num_finished++;
        }
        if(num_finished == num_nodes) {
            done = true;
        }
    };
    auto receiver_pred = [](const multicast_sst& sst) {
        return true;
    };
    auto num_times = window_size / num_nodes;
    if(!num_times) {
        num_times = 1;
    }
    auto receiver_trig = [=](multicast_sst& sst) {
        bool update_sst = false;
        for(uint i = 0; i < num_times; ++i) {
            for(uint j = 0; j < num_nodes; ++j) {
                uint32_t slot = sst.num_received_sst[node_id][j] % window_size;
                if((int64_t&)sst.slots[row_offset + j][(max_msg_size + 2 * sizeof(uint64_t)) * (slot + 1) - sizeof(uint64_t)] == (sst.num_received_sst[node_id][j]) / window_size + 1) {
                    sst_receive_handler(j, sst.num_received_sst[node_id][j],
                                        &sst.slots[row_offset + j][(max_msg_size + 2 * sizeof(uint64_t)) * slot],
                                        sst.slots[row_offset + j][(max_msg_size + 2 * sizeof(uint64_t)) * (slot + 1) - 2 * sizeof(uint64_t)]);
                    sst.num_received_sst[node_id][j]++;
                    update_sst = true;
                }
            }
        }
        if(update_sst) {
            sst.put(sst.num_received_sst.get_base() - sst.getBaseAddress(),
                    sizeof(sst.num_received_sst[0][0]) * num_nodes);
        }
    };
    sst->predicates.insert(receiver_pred, receiver_trig,
                           sst::PredicateType::RECURRENT);

    vector<uint32_t> indices(num_nodes);
    iota(indices.begin(), indices.end(), 0);
    std::vector<int> is_sender(num_nodes, 1);
    if(num_senders_selector == 0) {
    } else if(num_senders_selector == 1) {
        for(uint i = 0; i <= (num_nodes - 1) / 2; ++i) {
            is_sender[i] = 0;
        }
    } else {
        for(uint i = 0; i < num_nodes - 1; ++i) {
            is_sender[i] = 0;
        }
    }

    //Create group
    multicast_group<multicast_sst> g(sst, indices, window_size, max_msg_size, is_sender);

    if(node_rank == num_nodes - 1 || num_senders_selector == 0 || (node_rank > (num_nodes - 1) / 2 && num_senders_selector == 1)) {
        for(uint i = 0; i < num_messages; ++i) {
            volatile char* buf;
            while((buf = g.get_buffer(max_msg_size)) == NULL) {
            }
            clock_gettime(CLOCK_REALTIME, &send_times[i]);
            g.send();
        }
    }
    while(!done) {
    }

    //Gather results
    for(uint i = 0; i < num_senders; ++i) {
        stringstream ss;
        ss << "ml_received_" << num_nodes << "_" << i;
        ofstream fout;
        fout.open(ss.str());
        for(uint j = 0; j < num_messages; ++j) {
            fout << recv_times[i][j].tv_sec* (uint64_t)1e9 + recv_times[i][j].tv_nsec << endl;
        }
        fout.close();
    }

    stringstream ss;
    ss << "ml_sent_" << num_nodes << "_" << node_rank;
    ofstream fout;
    fout.open(ss.str());
    for(uint i = 0; i < num_messages; ++i) {
        fout << send_times[i].tv_sec* (uint64_t)1e9 + send_times[i].tv_nsec << endl;
    }
    fout.close();

    sst->sync_with_members();
}
