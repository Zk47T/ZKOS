Ở bài 3, chúng ta đã tiến hành tổ chức lại cấu trúc mã nguồn và xây dựng các hàm thư viện C cơ bản. Vì ứng dụng được chạy trực tiếp trên bare-metal (không có hệ điều hành), chúng ta không có sẵn các hàm chuẩn của C (`libc`). Điều này bắt buộc chúng ta phải tự triển khai chúng.

## 1. Tổ chức cấu trúc thư mục
Thay vì để tất cả vào một file `main.c` như các bài trước, mã nguồn của ZKOS sẽ bắt đầu được chia nhỏ ra cho gọn gàng và dễ quản lý:
- `include/`: Chứa các file header (`.h`) cung cấp nguyên mẫu hàm (prototypes) và định nghĩa macro. (Ví dụ: `uart.h`, `string.h`)
- `drivers/`: Chứa code liên tiếp trực tiếp với phần cứng, trong trường hợp này là `uart.c`.
- `lib/`: Chứa các hàm thư viện chung của hệ thống, không phụ thuộc vào nền tảng. Ở đây ta có `string.c`.

## 2. Thư viện string tự viết
Do trình biên dịch `gcc` cho bare-metal (sử dụng cờ `-ffreestanding`) sẽ yêu cầu những hàm có sẵn dùng để xử lý bộ nhớ và chuỗi khi chúng ta compile, chúng ta cần tự viết:
- `strlen()`: Tìm chiều dài của chuỗi, hữu ích khi in chuỗi hoặc xử lý bộ đệm.
- `strcmp()`: So sánh hai chuỗi, đây sẽ là hàm thiết yếu để hiện thực trình phân tích lệnh (Command Parser) cho shell ở Bài 4.
- `memset()`: Đặt đồ đồng loạt các byte trên một mảng/con trỏ về một giá trị duy nhất.
- `memcpy()`: Sao chép dữ liệu từ vùng nhớ này sang vùng nhớ khác.

Các hàm này có mô hình rất giống với các hàm chuẩn từ `string.h` của C.

## 3. Hàm in số Hex (`uart_puthex`)
Ở các bài trước chúng ta đã có `uart_putc()` (in ký tự) và `uart_puts()` (in chuỗi). Tuy nhiên, trên bare-metal, việc debug phần cứng thường xoay quanh việc đọc các giá trị Register (thường ở định dạng 32-bit hoặc 64-bit).
Để phục vụ việc đó, chúng ta viết thêm hàm `uart_puthex()`. Hàm này dịch (shift) tuần tự để lấy ra từng cụm 4 bits (nibble), chuyển đổi thành ký tự mã Hex, rồi in ra màn hình.

Việc chuẩn bị tốt những cơ sở này tạo một bước đệm hoàn hảo để tiếp tục phát triển giao diện tương tác cơ bản (Shell) với người dùng trong bài học tiếp theo.
