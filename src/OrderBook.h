//
// Created by gnome on 4/10/23.
//

#ifndef MY_PROJECT_ORDERBOOK_H
#define MY_PROJECT_ORDERBOOK_H

#include "SSLClient.h"
#include <iostream>

// TODO: 1. buffer from websocket stream
// TODO: 2. get depth snapshot 
// TODO: 3. drop any event where u < lastUpdateId in the snapshot
// TODO: 4. The first processed event should have U <= lastUpdateId AND u >= lastUpdateId
// TODO: 5. While listening to the stream, each new event's pu should be equal to the previous event's u, otherwise initialize the process from step 2. 
// TODO: 6 If quantity 0 remove the price level

class OrderBook {
public:
    struct Level {
        double price;
        double amount;
    };

    OrderBook();

    void run_forever(std::string &message);

    void depth_snapshot();

    void process_data();
    
    void buffer_data();

private:
    std::vector<Level> ask;
    std::vector<Level> bid;
    std::unique_ptr<SSLClient> ssl_client;
    
    std::vector<Level> parse_json(std::string json);
};


#endif //MY_PROJECT_ORDERBOOK_H
