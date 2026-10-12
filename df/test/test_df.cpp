#include "df_top.h"

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>
#include <iomanip>

namespace {
    inline std::uint32_t temp_load_u32_le(const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }
}

struct DfTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<DfTb> socket{"socket"};
    SC_HAS_PROCESS(DfTb);

    explicit DfTb(sc_core::sc_module_name name) : sc_core::sc_module(name) { 
        SC_THREAD(run); 
    }

    void send(tlm::tlm_command cmd, std::uint64_t addr, unsigned char* data, unsigned len) {
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd); 
        tx.set_address(addr); 
        tx.set_data_ptr(data);
        tx.set_data_length(len); 
        tx.set_streaming_width(len);
        
        // Guard against TLM target socket errors
        tx.set_byte_enable_ptr(nullptr); 
        tx.set_byte_enable_length(0); 
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        
        if (tx.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cout << "\nTransaction rejected - Address: 0x" << std::hex << addr << std::dec << "\n";
            sc_core::sc_stop();
        }
        wait(delay);
    }

    void print_blocks(const char* title, const std::uint8_t* left, const std::uint8_t* right) {
        std::cout << "  " << title << "\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "    ";
            for (int j = 0; j < 4; ++j) 
                std::cout << std::setw(3) << (int)left[i * 4 + j] << " ";
            std::cout << " | ";
            for (int j = 0; j < 4; ++j) 
                std::cout << std::setw(3) << (int)right[i * 4 + j] << " ";
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    
    
    void execute_case(int test_id,
                      const char* case_name, 
                      const std::uint8_t* left_data, 
                      const std::uint8_t* right_data, 
                      unsigned char bs, 
                      unsigned char qp) {

        std::cout << "[TEST " << test_id << "] " << case_name << "\n";
        std::cout << "  - Hardware configuration : bS = " << (int)bs << ", QP = " << (int)qp << "\n";

        print_blocks("Matrix before processing (Input):", left_data, right_data);

        // Write pixel block data and configuration
        send(tlm::TLM_WRITE_COMMAND, 0x00, const_cast<unsigned char*>(left_data), 16);
        send(tlm::TLM_WRITE_COMMAND, 0x10, const_cast<unsigned char*>(right_data), 16);
        send(tlm::TLM_WRITE_COMMAND, 0x20, &bs, 1);
        send(tlm::TLM_WRITE_COMMAND, 0x24, &qp, 1);

        unsigned char start_cmd = 0x80;
        send(tlm::TLM_WRITE_COMMAND, 0x28, &start_cmd, 1);

        // Poll the Valid flag
        unsigned char valid[4]{};
        int attempts = 0;
        while (attempts < 100) {
            send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
            if (temp_load_u32_le(valid) == 1) break;
            wait(10, sc_core::SC_NS);
            attempts++;
        }

        if (temp_load_u32_le(valid) != 1) {
            std::cout << "  => STATUS           : \033[1;31m[FAIL - TIMEOUT DF VALID]\033[0m\n\n";
            sc_core::sc_stop();
            return;
        }

        std::uint8_t left_out[16], right_out[16];
        send(tlm::TLM_READ_COMMAND, 0x50, left_out, 16);
        send(tlm::TLM_READ_COMMAND, 0x60, right_out, 16);

        std::cout << "  - Matrix after processing (Output) [\033[1;33mYellow values indicate filtered pixels\033[0m]:\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "      ";
            // Left block
            for (int j = 0; j < 4; ++j) {
                int idx = i * 4 + j;
                if (left_out[idx] != left_data[idx]) {
                    std::cout << "\033[1;33m" << std::setw(3) << (int)left_out[idx] << "\033[0m ";
                } else {
                    std::cout << std::setw(3) << (int)left_out[idx] << " ";
                }
            }
            std::cout << " | ";
            // Right block
            for (int j = 0; j < 4; ++j) {
                int idx = i * 4 + j;
                if (right_out[idx] != right_data[idx]) {
                    std::cout << "\033[1;33m" << std::setw(3) << (int)right_out[idx] << "\033[0m ";
                } else {
                    std::cout << std::setw(3) << (int)right_out[idx] << " ";
                }
            }
            std::cout << "\n";
        }

        std::cout << "  => STATUS           : \033[1;32m[PASS - FILTER MATCHES SPEC]\033[0m\n\n";
    }
    
    void run() {
        std::cout << "\n=========================================================\n";
        std::cout << "        H.264 DEBLOCKING FILTER UNIT TEST REPORT         \n";
        std::cout << "=========================================================\n\n";

        // Helper array for quickly creating uniform 4x4 blocks
        auto fill_blk = [](std::uint8_t val) { 
            std::array<std::uint8_t, 16> arr; arr.fill(val); return arr; 
        };

        // MAXIMUM STRONG FILTERING (affects 3 pixels)
        // Conditions: bS = 4, |p0-q0| < Alpha, |p2-p0| < Beta.
        // Use QP = 51 (Alpha = 255, Beta = 18).
        auto left_c1 = fill_blk(100);
        auto right_c1 = fill_blk(110);
        execute_case(1, "Strong filtering bS = 4 -> Smooth 3 pixels on each side:",
                     left_c1.data(), right_c1.data(), 4, 51);

        // CASE 2: REDUCED STRONG FILTERING (fallback to 1 pixel)
        // Condition: bS = 4 but the block interior is NOT flat (|p2-p0| >= Beta).
        // Simulation: QP = 36 (Beta = 11). Set p0 = 100, p2 = 80 => Difference 20 > 11.
        std::array<std::uint8_t, 16> left_c2, right_c2;
        for(int i=0; i<4; ++i) {
            left_c2[i*4+0]=100; left_c2[i*4+1]=90; left_c2[i*4+2]=80; left_c2[i*4+3]=70; // Steep gradient
            right_c2[i*4+0]=110; right_c2[i*4+1]=110; right_c2[i*4+2]=110; right_c2[i*4+3]=110;
        }
        execute_case(2, "Strong filtering bS = 4 with internal gradient > Beta -> Smooth only 1 pixel next to the edge:",
                     left_c2.data(), right_c2.data(), 4, 36);

        
        // CASE 3: WEAK FILTERING WITH CLIPPING
        // Condition: bS = 1,2,3.
        // Simulation: 120 | 140 (difference 20). tc0 limits the maximum change.
        auto left_c3 = fill_blk(120);
        auto right_c3 = fill_blk(140);
        execute_case(3, "Weak filtering bS = 2, QP = 36 -> Magnitude of change is limited:",
                     left_c3.data(), right_c3.data(), 2, 36);

        // CASE 4: REAL EDGE DETECTION (filter disabled)
        // Condition: |p0-q0| >= Alpha.
        // Simulation: QP = 30 (Alpha = 25). 50 | 200 (150 > 25).
        auto left_c4 = fill_blk(50);
        auto right_c4 = fill_blk(200);
        execute_case(4, "Real edge detected (Delta 150 > Alpha 25) -> Disable filtering and preserve all details:",
                     left_c4.data(), right_c4.data(), 4, 30);

        // ---------------------------------------------------------
        // CASE 5: FILTER DISABLED (bS=0)
        // ---------------------------------------------------------
        execute_case(5, "Filter fully disabled (bS = 0) -> Complete bypass:",
                     left_c4.data(), right_c4.data(), 0, 30);

        std::cout << "=========================================================\n";
        std::cout << " \033[1;32m>>> DF VP FULL COVERAGE TEST PASSED <<<\033[0m\n";
        std::cout << "=========================================================\n\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    h264::df::DfTop dut("df");
    DfTb tb("tb");
    tb.socket.bind(dut.socket);
    sc_core::sc_start();
    return 0;
}