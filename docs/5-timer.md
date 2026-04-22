Trong bài này, chúng ta đã tiếp cận hai System Modules là Watchdog Timer (WDOG3) và ARM Generic Timer của CPU A55 để giải quyết 2 bài toán: vô hiệu hóa tính năng tự động khởi động lại, và đo đạc thời gian của hệ thống.

## 1. Tắt Watchdog (WDOG3) - drivers/wdog.c

Nếu không có sự can thiệp nào, i.MX93 sẽ kích hoạt Watchdog 3 và nếu CPU không thực hiện việc "cho chó ăn" (Refresh hay Feed the watchdog) trong khoảng 40 giây, board sẽ tự reset. Điều này rất khó chịu khi viết code và test thủ công qua UART. Để khắc phục, ta căn cứ vào i.MX93 Reference Manual mục **69.6.1 Disabling WDOG**:

> To disable WDOG, you must first perform the unlock sequence. Then, write 0 to EN. The following code snippet shows an example of a 32-bit write.
> ```c
> DisableInterrupts; // disable global interrupt
> WDOG_CNT = 0xD928C520; //unlock watchdog
> WDOG_CS &= ~WDOG_CS_EN_MASK; //disable watchdog
> EnableInterrupts; //enable global interrupt
> ```

Dựa vào gợi ý phần cứng (hardware snippet) từ tài liệu, ta triển khai nó trong code C thực tế bằng cách tìm Base Address của `WDOG3` là `0x42490000`, tạo thao tác Unlock bằng Magic Word `0xD928C520` ghi thẳng vào thanh ghi Counter, rồi ghi tắt bit `EN` (bit 7) ở thanh ghi Control & Status (CS). Việc disable ngắt (Interrupt) tạm thời có thể bỏ qua vì chương trình bare-metal của chúng ta hiện thời còn chưa hề bật Interrupts! Hàm `wdog3_disable()` được gọi ngay ở đầu hàm `main()` để "diệt" watchdog từ trong trứng nước.

## 2. ARM Generic Timer (Giao tiếp qua ASM) - drivers/timer.c

Để dùng được lệnh delay (`sleep_ms`) và tính `uptime`, ta không dùng timer cứng của SoC (như GPT hay PIT liên kết qua các vùng nhớ) mà dùng module ARM Generic Timer tích hợp thẳng bên trong nhân Cortex-A55.

**Tại sao lại phải dùng ASM ở đây mà không trỏ pointer bằng C?**
Khác với Watchdog hay UART thuộc loại thiết bị ngoại vi ánh xạ vào bộ nhớ (Memory-Mapped I/O - MMIO) cho phép truy cập dễ dàng bằng con trỏ định vị địa chỉ trong ngôn ngữ C (ví dụ `0x42490000`); các thanh ghi của Generic Timer như `CNTPCT_EL0` lại là **System Registers** (Thanh ghi Hệ thống). Chúng hoàn toàn **không có địa chỉ vật lý** nằm ở vùng nhớ ngoài, mà nằm khảm sâu trong kiến trúc lõi vi xử lý. 
Cách duy nhất để đọc hoặc tuỳ chỉnh System Registers là dùng các tập lệnh Assembly chuyên dụng của dòng ARMv8-A: lệnh `mrs` (đọc) và `msr` (ghi).

> **Sự tương đồng với thanh ghi `sp` trong `stub.S`:**
> Hiện tượng này hoàn toàn cực kì logic và tương đồng với việc khởi tạo thanh ghi Stack Pointer (`sp`) ở Bài 1 c-lang. Thanh ghi `sp` cũng là một thanh ghi nội tại của CPU, không có địa chỉ MMIO. Bởi vậy ta buộc phải khởi tạo nó trong một tệp pure Assembly (`stub.S`) bằng chỉ thị ASM trước khi mã nguồn nhảy sang không gian của C thuần tuý (`main.c`) được. Để giữ code `timer.c` gọn gàng, ta bọc cú pháp lằng nhằng của `asm volatile` lại thành một Macro có dạng `READ_SYSREG()`.

Cortex-A55 cung cấp các thanh ghi System Registers quan trọng sau cho Timer:
- `CNTPCT_EL0`: Chứa bộ đếm cycle tăng liên tục kể từ khi cấp nguồn.
- `CNTFRQ_EL0`: Chứa tần số mà Timer đang chạy (U-Boot và TF-A đã đóng vai trò khởi tạo giúp ta lúc boot chain chạy). Tại sao ta cần cái này? Vì `CNTPCT_EL0` chỉ trả về **số lượng ticks (bước đếm)**. Ta không biết 1 tick mất bao nhiêu giây nếu không biết Timer đang chạy ở tần số bao nhiêu. Ví dụ, nếu `CNTFRQ_EL0` là 24,000,000 (24MHz), nghĩa là mỗi giây CPU có 24 triệu ticks. Nhờ lấy số lượng phân giải đó, ta mới quy ra được 1 giây dài bằng bao nhiêu tick.

Bằng cách biết xung nhịp (Frequency) hiện tại, ta có thể dễ dàng delay (chờ đợi một lượng tick nhất định) cũng như tính CPU `uptime` theo giây cực kì chuẩn xác.

**Giải thích về cơ chế `sleep_ms` và `while` spin:**
Hàm `sleep_ms()` tính số ticks cần phải chờ (bằng cách quy đổi từ số mili-giây sang ticks nhờ dùng Tần số / 1000). 
Sau khi lưu lại mốc thời gian lúc bắt đầu (start), chương trình thực hiện một vòng lặp `while` trống (còn gọi là *spin-wait* hoặc *busy-wait*). Vòng lặp này sẽ ngốn 100% CPU, liên tục đọc lại `CNTPCT_EL0` để kiểm tra xem "thời gian hiện tại trừ đi lúc bắt đầu đã đạt đủ lượng tick mục tiêu (điều kiện) hay chưa". Đây là cách làm phổ biến trong Bare-metal thuở sơ khai trước khi có Interrupts để báo thức.

## 3. Hàm xuất số thập phân (uart_putint)

Vì uptime xuất ra giây tự nhiên sẽ dùng số hệ 10 (decimal), ta đã nâng cấp thư viện uart để bổ sung hàm `uart_putint()`.
Hàm này áp dụng kiến thức cơ bản về toán học tách các chữ số bằng phép chia/module rồi sử dụng mảng phụ để in ngược các chữ số ra màn hình theo đúng chiểu hiển thị ASCII.

## Kết quả mong đợi

Lần này khi nạp chương trình vào Board, bạn có thể:
1. Thảnh thơi gõ code ngồi đọc console mà không sợ Board thình lình bị reset sau 40 giây.
2. Gõ `uptime` và chiêm ngưỡng số giây board đã sống sót chạy chương trình tính đến lúc đó:
```text
ZKOS> uptime
Uptime: 14 seconds
```
