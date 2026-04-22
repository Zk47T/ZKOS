Ở bài trước chúng ta đã nhận string vào buffer. Bây giờ, với sự hỗ trợ của các hàm string tự viết, ta sẽ xây dựng một *Simple Shell* (trình vỏ tương tác dòng lệnh) có khả năng đọc lệnh, phân tích lệnh (parse command) và thực thi. Chức năng chính sẽ bao gồm việc hiển thị dấu nhắc lệnh (`ZKOS>`) và phản hồi các lệnh.

## 1. Line buffer và Command Parsing

Trong shell liên tục đọc dữ liệu theo ký tự:
- Nhận các ký tự thông thường và lưu vào biến `buf`.
- Xử lý phím `Backspace` (`\b` hoặc `0x7F`) để xoá ký tự đang gõ, cho phép sửa lỗi trước khi gửi. 
  - **Lưu ý về hiển thị xóa (`\b \b`)**: Ký tự điều khiển `\b` gửi tới Terminal thực chất chỉ dời con trỏ (cursor) lui sang trái 1 bước mà "không xóa" ký tự trên màn hình. Để "xóa" thật sự về mặt hiển thị, thuật toán cần gửi đi bộ chuẩn 3 thao tác `\b \b` bao gồm:
    1. Gửi `\b`: Lùi con trỏ đè lên ký tự cuối cùng.
    2. Gửi dấu cách (`' '`): In đè khoảng trắng lên chữ đó để làm nó biến mất (khi in dấu cách con trỏ tự động nhảy sang phải do thêm ký tự mới).
    3. Gửi `\b` lần nữa: Lùi con trỏ về lại vị trí đích để sẵn sàng gõ chữ tiếp theo.
- Khi người dùng nhấn Enter (`\r`), kết thúc chuỗi bằng `\0` rồi tiến hành phân tích câu lệnh.

## 2. Hàm xử lý lệnh (execute_command)

Lệnh được phân tích bằng cách sử dụng `strcmp()` (hàm đã viết trong thư viện string):
- `help`: In ra danh sách các lệnh có sẵn để người dùng tham khảo.
- `hello`: In ra lời chào `Hello from ZKOS!`.
- `info`: In ra thông tin tác giả và hệ thống với nội dung "ZKOS - author Nguyen Minh Tien - date 21/04/2026".

## 3. Quản lý trạng thái và Bộ đệm

Sau khi lệnh được chạy, bộ đệm (`buf`) sẽ được reset (thông qua biến index `i=0`), và màn hình lại in ra dấu nhắc lệnh `ZKOS> ` chờ đợi người dùng nhập lệnh tiếp theo. Đây là cách mô tả cơ bản nhất về một State Machine (Máy trạng thái) kiểm soát quá trình nhập liệu.

## Kết quả Mong đợi

Khi flash vào board và sử dụng trình console (như minicom/picocom), bạn sẽ thấy kết quả:

```text
Welcome to ZKOS Shell
Type 'help' to see available commands.

ZKOS> help
Available commands:
  help   - Show this help message
  hello  - Print a greeting
  info   - System information
ZKOS> info
ZKOS - author Nguyen Minh Tien - date 21/04/2026
ZKOS> 
```

Bài thực hành này giúp chúng ta hình dung cách giao tiếp giữa User Interface và Kernel của hệ thống nhúng trong môi trường không có OS. Đây chính là nền tảng khởi đầu của một Command Line Interface.
