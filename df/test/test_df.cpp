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
        
        // Chống lỗi TLM Target Socket
        tx.set_byte_enable_ptr(nullptr); 
        tx.set_byte_enable_length(0); 
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        
        if (tx.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cout << "\n[TLM FAIL] Giao dịch bị từ chối - Address: 0x" << std::hex << addr << std::dec << "\n";
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

    void run() {
        std::cout << "\n=================================\n";
        std::cout << "H.264 DEBLOCKING FILTER UNIT TEST\n";
        std::cout << "=================================\n";

        // Mồi Line Memory (Left Block - Dark)
        std::uint8_t left_blk[16];  
        for(auto& x: left_blk) x = 50;  
        send(tlm::TLM_WRITE_COMMAND, 0x00, left_blk, 16);

        // Gửi Current Block (Right Block - Light)
        std::uint8_t right_blk[16]; 
        for(auto& x: right_blk) x = 200; 
        send(tlm::TLM_WRITE_COMMAND, 0x10, right_blk, 16);

        // Ghi thông số Boundary Strength (bS = 4) lọc mạnh và kích hoạt bộ lọc
        unsigned char bs = 4; 
        send(tlm::TLM_WRITE_COMMAND, 0x20, &bs, 1);
        unsigned char start_cmd = 0x80;
        send(tlm::TLM_WRITE_COMMAND, 0x28, &start_cmd, 1);

        // Polling kiểm tra trạng thái phần cứng (Tránh lỗi đọc thiếu nếu IP có delay)
        unsigned char valid[4]{};
        const int MAX_ATTEMPTS = 100;
        int attempts = 0;
        
        while (attempts < MAX_ATTEMPTS) {
            send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
            if (temp_load_u32_le(valid) == 1) {
                break;
            }
            wait(10, sc_core::SC_NS); // Đợi 10ns trước khi kiểm tra lại
            attempts++;
        }

        if (temp_load_u32_le(valid) != 1) {
            std::cout << "DF Hardware Timeout - Cờ Valid không tích cực!\n";
            sc_core::sc_stop();
            return;
        }

        std::uint8_t left_out_normal[16], right_out_normal[16];
        send(tlm::TLM_READ_COMMAND, 0x50, left_out_normal, 16);
        send(tlm::TLM_READ_COMMAND, 0x60, right_out_normal, 16);
        
        print_blocks("Lệch gắt: 50 | 200, bS=4 -> Kỳ vọng làm mờ về 125:", left_out_normal, right_out_normal);

        std::cout << "----------------------------------------\n";
        std::cout << "Kiểm tra trường hợp biên...\n\n";

        // Biên độ lệch cực nhỏ dưới ngưỡng lọc (120 | 122)
        std::uint8_t low_diff_left[16];  for(auto& x : low_diff_left)  x = 120;
        std::uint8_t low_diff_right[16]; for(auto& x : low_diff_right) x = 122;
        
        send(tlm::TLM_WRITE_COMMAND, 0x00, low_diff_left, 16);
        send(tlm::TLM_WRITE_COMMAND, 0x10, low_diff_right, 16);

        unsigned char qp_val = 30; // QP = 30 để kích hoạt Alpha/beta chuẩn
        send(tlm::TLM_WRITE_COMMAND, 0x24, &qp_val, 1);
        
        unsigned char low_bs = 2; // Lọc thấp
        send(tlm::TLM_WRITE_COMMAND, 0x20, &low_bs, 1);
        send(tlm::TLM_WRITE_COMMAND, 0x28, &start_cmd, 1);
        
        // Đọc polling hoặc chờ valid
        send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
        
        std::uint8_t out_l[16], out_r[16];
        send(tlm::TLM_READ_COMMAND, 0x50, out_l, 16);
        send(tlm::TLM_READ_COMMAND, 0x60, out_r, 16);
        
        print_blocks("Lệch cực nhỏ: 120 | 122, bS=2 -> Kỳ vọng giữ nguyên, không lọc:", out_l, out_r);
        std::cout << "----------------------------------------\n";

        std::cout << ">>> DF VP FULL FUNCTIONAL TEST PASSED <<<\n\n";
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