#ifndef EXCEPTION_H
#define EXCEPTION_H

#include "types.h"

// Cấu trúc lưu trữ context của CPU khi xảy ra ngoại lệ
// Số liệu này được push vào Stack bởi tệp ASM `exceptions.S`
struct exception_context {
    uint64_t x[31];   // Các thanh ghi đa dụng x0 - x30 (248 bytes)
    uint64_t esr;     // Exception Syndrome Register (Mã lỗi/Lý do lỗi)
    uint64_t elr;     // Exception Link Register (Địa chỉ gây lỗi)
    uint64_t padding; // Đệm để giữ cấu trúc căn lề đúng 16 byte (Tổng: 272 bytes)
};

// Hàm xử lý chung bằng ngôn ngữ C
void trap_handler(struct exception_context *ctx);

#endif
