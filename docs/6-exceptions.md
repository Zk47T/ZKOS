# Bài 6 - Exception Vectors: Hành trình giải mã "Kernel Panic"

Càng đi sâu vào Bare-metal, mọi thứ bắt đầu phức tạp hơn và không còn chỉ là chạy code tuyến tính nữa. Dưới đây không chỉ là giải thích các dòng code, mà còn là **hành trình tư duy** từ lúc nhận đề bài cho đến khi viết ra từng dòng lệnh cho hệ thống Bắt lỗi (Exception Myt).

---

## Bối cảnh và Đề Bài
**Vấn đề:** Ở các bài trước, nếu bạn viết code bị lỗi (như trỏ con trỏ vào địa chỉ cấm `0x0`), CPU Cortex-A55 sẽ từ chối thực thi và... "tắt điện". Board im lìm, UART câm nín, bạn không biết code của mình chết ở dòng nào. 
**Đề bài:** Làm sao để khi có lỗi, CPU có thể hét lên: *"Tôi bị lỗi ở địa chỉ XYZ, và biến X0 lúc đó đang có giá trị này..."* (giống màn hình xanh của Windows hay Kernel Panic của Linux)?

---

## Quá trình suy luận và Tìm hiểu

### Bước 1: "CPU báo lỗi cho ai?" - Đi tìm Vector Table
* **Nghiên cứu:** Ta mở cẩm nang ARMv8 Architecture Reference Manual. Tài liệu chỉ ra rằng, mỗi khi có Exception (lỗi rẽ nhánh khẩn cấp), phần cứng sẽ ngay lập tức bỏ dở mọi thứ, tự động trỏ thanh ghi `PC` (Program Counter) nhảy tới một địa chỉ đặc biệt. Địa chỉ đó được lưu trong thanh ghi hệ thống tên là `VBAR_EL2` (Vector Base Address Register ở mức đặc quyền EL2).
* **Luận điểm:** Vậy nhiệm vụ đầu tiên là: Tạo ra một "Bảng Bắt Lỗi" và nạp địa chỉ của nó vào `VBAR_EL2` từ sớm nhất có thể.
* **Hành động (`stub.S`):** Cứu tinh của chúng ta bắt đầu ngay từ lúc khởi động, trước khi nhảy vào hàm `main`:
    ```assembly
    // Lấy địa chỉ của bảng (do ta tự định nghĩa ở dưới) nạp vào Register
    ldr x0, =vector_table_el2
    msr vbar_el2, x0 
    ```

### Bước 2: Bảng Vector vẽ như thế nào? (`vectors.S`)
* **Nghiên cứu:** Kiến trúc ARM yêu cầu cái bảng `vector_table_el2` không được làm tuỳ tiện. Kích thước bảng phải chính xác **2KB** (được biểu thị bằng `.align 11`). Bên trong bảng có 16 "căn phòng", CPU sẽ tuỳ loại lỗi mà bay vào căn phòng tương ứng. Mỗi phòng chứa đúng 128 bytes (`.align 7`).
* **Hành động:** Ta sẽ thiết kế cái "chung cư" 16 phòng đó. Đối với Bare-metal chạy ở EL2, lỗi code C (phân loại là lỗi *Synchronous*) sẽ rơi vào phòng số 5 (Current EL with SPx). Ta viết đoạn ASM sau:

    ```assembly
    .align 11               // Khởi công chung cư 2KB
    vector_table_el2:

        // 4 phòng đầu tiên bỏ qua
        .align 7; b . ; .align 7; b . ; .align 7; b . ; .align 7; b . 
    
        // Nơi ZKOS sẽ bắt lỗi !!
        .align 7            // Align 128 bytes
        b handle_sync_el2   // Lập rào chắn, phi ngay vào hàm xử lý của ta
    ```

### Bước 3: "Bảo vệ hiện trường" bằng Stack 
* **Vấn đề cốt lõi:** Khi nhảy sang `handle_sync_el2`, bản chất hàm C sẽ sử dụng các thanh ghi trung gian `x0, x1...` để tính toán in ra chữ. Nếu ta nhảy vào C luôn, hàm `printf` hoặc `uart_puts` sẽ ghi đè lên các thanh ghi `x0` cũ của hệ thống! Dấu vết của vụ án bị xoá sạch.
* **Giải pháp:** Phải viết ASM để "đông lạnh" mọi thứ lập tức, cất hết 30 thanh ghi `x0-x30` vào Stack. Tức là mở rộng không gian RAM (sp) rồi cất đồ.
* **Hành động:** Tính nhẩm -> 31 thanh ghi (8 bytes) + ESR (8 bytes) + ELR (8 bytes) = Xấp xỉ 272 bytes. Vì quy tắc của Stack trên ARM là chia hết cho 16, nên con số 272 là chuẩn xác. Ta viết macro:
    ```assembly
    sub sp, sp, #272                 // Dịch stack xuống đáy 272 bước
    stp x0, x1, [sp, #16 * 0]        // Xếp gọn gàng x0, x1 vào RAM
    ...
    mrs x0, esr_el2                  // Lôi bí mật "lý do chết" từ System
    mrs x1, elr_el2                  // Lôi bí mật "địa chỉ tự tử"
    stp x0, x1, [sp, #248]           // Xếp lấp nốt vào cuối Stack
    ```

### Bước 4: Móc ghép RAM từ ASM sang C Struct
* **Vấn đề:** 272 bytes kia nằm thành đống trong RAM (tại đầu con trỏ `sp`). Bằng cách nào mã C biết được từng byte một đại diện cho cái gì để mà đọc?
* **Giải pháp:** Cực kỳ vi diệu, tính chất của **C Struct** là ánh xạ bộ nhớ tịnh tiến. Ta chỉ cần định nghĩa ra một cái Struct có thứ tự và kích cỡ *y hệt* như lúc ta xếp đồ trên ASM.
* **Hành động (`exception.h`):** Ta khai báo C struct (ánh xạ chính xác cấu trúc Stack):
    ```c
    struct exception_context {
        uint64_t x[31];   // 31 thanh ghi, tốn 248 bytes (chiếm offset từ 0 -> 247)
        uint64_t esr;     // Đúng thứ tự offset 248!
        uint64_t elr;     // Offset 256!
        uint64_t padding; // Cho chẵn 272 bytes
    };
    ```

### Bước 5: Sang số và Chuyển quyền cho C
* **Hành động:** Sau khi đóng gói dữ liệu xong xuôi trên RAM, ta chuyển thẻ nhớ `sp` chứa dữ liệu qua tham số C (nằm ở `x0` - theo tiêu chuẩn của ARM) rồi tung quyền cho C:
    ```assembly
    handle_sync_el2:
        save_all_regs       // Cất hết vào Stack (RAM)
        mov x0, sp          // Cầm con trỏ RAM mang cấu trúc chứa 272 bytes gán vào tham số x0
        bl trap_handler     // Sang C code xử lý Dump
    ```

Mọi thứ đến hàm `trap_handler(struct exception_context *ctx)` trong C thì trở thành dọn cỗ sẵn. Bạn đã có đủ `ctx->x[0]`, `ctx->esr`, `ctx->elr`. Giờ chỉ việc in ra màn hình và tận hưởng!

Nhờ quá trình suy luận từ gốc rễ này, ZKOS đã kết nối chặt chẽ và nhịp nhàng giữa Kiến trúc phần cứng ARM, Logic Stack của Assembly và Phép ánh xạ của ngôn ngữ C!
