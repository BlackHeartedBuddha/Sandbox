#include <rtc/rtc.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <queue>

const char* stateToString(rtc::PeerConnection::State state) {
    switch (state) {
        case rtc::PeerConnection::State::New: return "New";
        case rtc::PeerConnection::State::Connecting: return "Connecting";
        case rtc::PeerConnection::State::Connected: return "Connected";
        case rtc::PeerConnection::State::Disconnected: return "Disconnected";
        case rtc::PeerConnection::State::Failed: return "Failed";
        case rtc::PeerConnection::State::Closed: return "Closed";
        default: return "Unknown";
    }
}

int main() {
    try {
        // Initialize logging
        rtc::InitLogger(rtc::LogLevel::Info);

        rtc::Configuration config;
        // Use Google's public STUN servers
        config.iceServers.emplace_back("stun:stun.l.google.com:19302");
        config.iceServers.emplace_back("stun:stun1.l.google.com:19302");
        config.iceServers.emplace_back("stun:stun2.l.google.com:19302");
        
        // Add port range for better NAT traversal
        config.portRangeBegin = 10000;
        config.portRangeEnd = 10100;

        // Create two PeerConnections: caller and callee
        auto caller = std::make_shared<rtc::PeerConnection>(config);
        auto callee = std::make_shared<rtc::PeerConnection>(config);

        // Synchronization primitives
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic<bool> callerConnected{false};
        std::atomic<bool> calleeConnected{false};
        std::atomic<bool> dataChannelReady{false};
        std::atomic<bool> offerReady{false};
        std::atomic<bool> answerReady{false};
        std::atomic<bool> remoteDescriptionSet{false};

        std::string offerSdp;
        std::string answerSdp;

        // Store ICE candidates until remote description is set
        std::queue<rtc::Candidate> callerCandidates;
        std::queue<rtc::Candidate> calleeCandidates;
        std::mutex candidatesMtx;

        // ICE candidate handling with proper timing
        caller->onLocalCandidate([&](rtc::Candidate candidate) {
            std::cout << "[Caller] Generated ICE candidate: " << std::string(candidate) << std::endl;
            std::lock_guard<std::mutex> lock(candidatesMtx);
            callerCandidates.push(candidate);
        });
        
        callee->onLocalCandidate([&](rtc::Candidate candidate) {
            std::cout << "[Callee] Generated ICE candidate: " << std::string(candidate) << std::endl;
            std::lock_guard<std::mutex> lock(candidatesMtx);
            calleeCandidates.push(candidate);
        });

        // Log state changes
        caller->onStateChange([&](rtc::PeerConnection::State state) {
            std::cout << "[Caller] Connection State: " << stateToString(state) << std::endl;
            callerConnected = (state == rtc::PeerConnection::State::Connected);
            cv.notify_all();
        });
        
        callee->onStateChange([&](rtc::PeerConnection::State state) {
            std::cout << "[Callee] Connection State: " << stateToString(state) << std::endl;
            calleeConnected = (state == rtc::PeerConnection::State::Connected);
            cv.notify_all();
        });

        // Set up local description callbacks
        caller->onLocalDescription([&](rtc::Description description) {
            std::cout << "[Caller] Local description created (offer)" << std::endl;
            offerSdp = std::string(description);
            offerReady = true;
            cv.notify_all();
        });

        callee->onLocalDescription([&](rtc::Description description) {
            std::cout << "[Callee] Local description created (answer)" << std::endl;
            answerSdp = std::string(description);
            answerReady = true;
            cv.notify_all();
        });

        // Caller creates a data channel
        auto dcCaller = caller->createDataChannel("test");

        dcCaller->onOpen([&]() {
            std::cout << "[Caller] Data channel open!" << std::endl;
            dataChannelReady = true;
            cv.notify_all();
        });

        dcCaller->onMessage([](rtc::message_variant msg) {
            std::visit([](auto&& data) {
                using T = std::decay_t<decltype(data)>;
                if constexpr(std::is_same_v<T, std::string>) {
                    std::cout << "[Caller] Message received: " << data << std::endl;
                } else if constexpr(std::is_same_v<T, rtc::binary>) {
                    std::cout << "[Caller] Binary message received, size: " << data.size() << std::endl;
                }
            }, msg);
        });

        dcCaller->onError([](std::string error) {
            std::cout << "[Caller] Data channel error: " << error << std::endl;
        });

        // Store callee data channel reference
        std::shared_ptr<rtc::DataChannel> dcCallee;

        // Callee listens for data channel creation
        callee->onDataChannel([&](std::shared_ptr<rtc::DataChannel> dc) {
            std::cout << "[Callee] Data channel created!" << std::endl;
            dcCallee = dc;

            dcCallee->onOpen([&]() {
                std::cout << "[Callee] Data channel open!" << std::endl;
                cv.notify_all();
            });

            dcCallee->onMessage([](rtc::message_variant msg) {
                std::visit([](auto&& data) {
                    using T = std::decay_t<decltype(data)>;
                    if constexpr(std::is_same_v<T, std::string>) {
                        std::cout << "[Callee] Message received: " << data << std::endl;
                    } else if constexpr(std::is_same_v<T, rtc::binary>) {
                        std::cout << "[Callee] Binary message received, size: " << data.size() << std::endl;
                    }
                }, msg);
            });

            dcCallee->onError([](std::string error) {
                std::cout << "[Callee] Data channel error: " << error << std::endl;
            });
        });

        // SDP exchange with proper sequencing
        std::cout << "Starting SDP exchange..." << std::endl;

        // Step 1: Caller creates offer
        std::cout << "[Caller] Creating offer..." << std::endl;
        caller->setLocalDescription();

        // Wait for offer to be ready
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (!cv.wait_for(lock, std::chrono::seconds(10), [&]() { return offerReady.load(); })) {
                std::cerr << "Failed to create offer within timeout" << std::endl;
                return 1;
            }
        }

        std::cout << "[Caller] Offer ready, sending to callee..." << std::endl;

        // Step 2: Set offer as remote description for callee
        rtc::Description offer(offerSdp, rtc::Description::Type::Offer);
        callee->setRemoteDescription(offer);
        std::cout << "[Callee] Remote description (offer) set" << std::endl;

        // Step 3: Callee creates answer
        std::cout << "[Callee] Creating answer..." << std::endl;
        callee->setLocalDescription();

        // Wait for answer to be ready
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (!cv.wait_for(lock, std::chrono::seconds(10), [&]() { return answerReady.load(); })) {
                std::cerr << "Failed to create answer within timeout" << std::endl;
                return 1;
            }
        }

        std::cout << "[Callee] Answer ready, sending to caller..." << std::endl;

        // Step 4: Set answer as remote description for caller
        rtc::Description answer(answerSdp, rtc::Description::Type::Answer);
        caller->setRemoteDescription(answer);
        std::cout << "[Caller] Remote description (answer) set" << std::endl;

        // Now exchange ICE candidates after remote descriptions are set
        std::cout << "Exchanging ICE candidates..." << std::endl;
        
        // Add caller's candidates to callee
        {
            std::lock_guard<std::mutex> lock(candidatesMtx);
            while (!callerCandidates.empty()) {
                auto candidate = callerCandidates.front();
                callerCandidates.pop();
                std::cout << "[Caller->Callee] Adding ICE candidate" << std::endl;
                callee->addRemoteCandidate(candidate);
            }
        }

        // Add callee's candidates to caller
        {
            std::lock_guard<std::mutex> lock(candidatesMtx);
            while (!calleeCandidates.empty()) {
                auto candidate = calleeCandidates.front();
                calleeCandidates.pop();
                std::cout << "[Callee->Caller] Adding ICE candidate" << std::endl;
                caller->addRemoteCandidate(candidate);
            }
        }

        // Set up continuous candidate exchange for any new candidates
        std::thread candidateExchanger([&]() {
            while (!callerConnected || !calleeConnected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                {
                    std::lock_guard<std::mutex> lock(candidatesMtx);
                    
                    // Exchange caller candidates
                    while (!callerCandidates.empty()) {
                        auto candidate = callerCandidates.front();
                        callerCandidates.pop();
                        callee->addRemoteCandidate(candidate);
                    }
                    
                    // Exchange callee candidates
                    while (!calleeCandidates.empty()) {
                        auto candidate = calleeCandidates.front();
                        calleeCandidates.pop();
                        caller->addRemoteCandidate(candidate);
                    }
                }
            }
        });

        std::cout << "SDP exchange completed. Waiting for connection..." << std::endl;

        // Wait for connection to establish with longer timeout
        auto start = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(60); // Increased timeout
        
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, timeout, [&]() { 
                return callerConnected.load() && calleeConnected.load(); 
            });
        }

        candidateExchanger.join();

        if (callerConnected && calleeConnected) {
            std::cout << "Connection established successfully!" << std::endl;
            
            // Wait for data channel to be ready
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait_for(lock, std::chrono::seconds(10), [&]() { 
                    return dataChannelReady.load(); 
                });
            }
            
            if (dataChannelReady) {
                std::cout << "Data channel ready! Starting message exchange..." << std::endl;
                
                // Give it a moment to stabilize
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                // Send messages back and forth
                for (int i = 0; i < 5; i++) {
                    if (dcCaller && dcCaller->isOpen()) {
                        std::string msg = "Hello from caller #" + std::to_string(i);
                        dcCaller->send(msg);
                        std::cout << "[Caller] Sent: " << msg << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    
                    if (dcCallee && dcCallee->isOpen()) {
                        std::string msg = "Hello from callee #" + std::to_string(i);
                        dcCallee->send(msg);
                        std::cout << "[Callee] Sent: " << msg << std::endl;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                
                // Keep running for a bit to see all messages
                std::cout << "Message exchange complete. Keeping connection alive for 5 more seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            } else {
                std::cout << "Data channel failed to open within timeout" << std::endl;
            }
        } else {
            std::cout << "Connection failed to establish within timeout" << std::endl;
            std::cout << "Caller connected: " << callerConnected << std::endl;
            std::cout << "Callee connected: " << calleeConnected << std::endl;
        }

        std::cout << "Shutting down..." << std::endl;
        
        // Clean shutdown
        if (dcCaller) dcCaller->close();
        if (dcCallee) dcCallee->close();
        caller->close();
        callee->close();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}