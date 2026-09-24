#ifndef DOWNLOADS_CLEANER_H
#define DOWNLOADS_CLEANER_H

#include "CleanerCore.h"
#include <unordered_set>
#include <string>

/**
 * @brief Module dọn dẹp an toàn & thông minh thư mục Downloads:
 * 
 * 1. NGUYÊN TẮC AN TOÀN TUYỆT ĐỐI (WHITELIST & PHÂN BIỆT XÓA):
 *    - CHỈ xử lý đúng 4 nhóm file:
 *      + File tải dở dang/lỗi: .crdownload, .part, .tmp (quá 24h)
 *        ==> XÓA CỨNG TRIỆT ĐỂ (hard delete vì không có giá trị khôi phục).
 *      + File cài đặt: .exe, .msi
 *      + File ảnh: .jpg, .jpeg, .png, .gif, .webp, .bmp, .svg, .ico, .tiff
 *      + File video: .mp4, .mkv, .avi, .mov, .wmv, .flv, .webm, .m4v
 *        ==> XÓA MỀM (soft delete đưa vào Thùng rác, tạo điều kiện lấy lại file).
 *    - TUYỆT ĐỐI KHÔNG chạm vào bất kỳ file nào khác (tài liệu, nén zip/rar, code...).
 *    - THỨ TỰ THIẾT KẾ: Trong chuỗi tự động, lệnh dọn Downloads luôn CHẠY CUỐI CÙNG
 *      sau khi hệ thống đã xóa sạch sành sanh Thùng rác trước đó!
 * 
 * 2. CƠ CHẾ DỌN FILE CÀI ĐẶT (.exe, .msi):
 *    - Đọc PE Header Version Info (ProductName, FileDescription)
 *    - Đối chiếu Registry phần mềm đã cài đặt trên Windows
 *    - Nếu ứng dụng ĐÃ ĐƯỢC CÀI ĐẶT -> Chuyển file cài đặt vào Thùng rác
 *    - Bảo vệ ứng dụng dạng portable, standalone.
 * 
 * 3. CƠ CHẾ DỌN FILE TRÙNG LẶP (.exe, .msi, ảnh, video):
 *    - Nhận diện các bản sao được Windows đánh số: file (1).ext, file (2).ext, file (5).ext
 *    - Quy tắc giữ file: CHỈ GIỮ file gốc (file.ext) VÀ file có chỉ số cao nhất (file (5).ext)
 *    - Toàn bộ các file bản sao ở giữa (file (1), file (2)...) được chuyển vào Thùng rác.
 */
class DownloadsCleaner {
private:
    static std::string getDownloadsPath();
    static std::unordered_set<std::string> getInstalledAppNames();
    static std::string getExeProductName(const std::string& exePath);
    static std::string cleanAppName(const std::string& raw);

public:
    static bool isSupportedExecutable(const std::string& ext);
    static bool isSupportedImage(const std::string& ext);
    static bool isSupportedVideo(const std::string& ext);
    static bool isCorruptDownload(const std::string& ext);

    static CleanStats clean(bool dryRun = false);
};

#endif // DOWNLOADS_CLEANER_H
