# agent.md — Hướng dẫn cho Codex/Agent làm việc trong repo này

## 0) Mục tiêu tổng quát
Bạn (Agent) cần thực hiện 3 nhóm việc chính:

1) **Kiểm tra và xử lý bug toàn dự án**
- Dò lỗi compile/build, runtime, logic, crash, memory leak, race-condition (nếu có).
- Ưu tiên: lỗi gây crash/không chạy được/không kết nối được WebSocket > lỗi logic > lỗi UX.

2) **Cải thiện UI/UX phần web (hiện đại, dễ dùng, rõ trạng thái)**
- Làm UI trông “modern”, gọn gàng, có hệ thống, responsive.
- Nâng trải nghiệm: trạng thái kết nối, lỗi/notification, loading, empty-state, layout, typography.

3) **Xử lý “hoàn chỉnh” các file trong danh sách phạm vi dưới đây**
- Đọc kỹ, refactor hợp lý, sửa lỗi, cải thiện cấu trúc, thêm type/validation, dọn code smell.
- Nếu cần sửa các file ngoài danh sách để build/test chạy được thì được phép, nhưng **tối thiểu thay đổi**.

---

## 1) Phạm vi file bắt buộc xử lý
### Root
- `list.h`, `list.cpp`
- `start.h`, `start.cpp`
- `end.h`, `end.cpp`
- `main.cpp`
- `keylogger.h`, `keylogger.cpp`
- `CMakeLists.txt`
- `CMakePresets.json`
- `.vscode/c_cpp_properties.json`, `.vscode/settings.json`

### C++ (include/)
- `include/utils/json.hpp`
- `include/utils/base64.hpp`
- `include/modules/system_control.hpp`
- `include/modules/screen.hpp`
- `include/modules/process.hpp`
- `include/modules/consent.hpp`
- `include/modules/camera.hpp`
- `include/core/command.hpp`
- `include/core/dispatcher.hpp`
- `include/network/ws_server.hpp`
- `include/network/ws_client.hpp`

### C++ (src/)
- `src/server/main.cpp`
- `src/client/main.cpp`
- `src/modules/system_control.cpp`
- `src/modules/consent.cpp`
- `src/modules/screen.cpp`
- `src/modules/process.cpp`
- `src/modules/camera.cpp`
- `src/utils/base64.cpp`
- `src/network/ws_client.cpp`
- `src/network/ws_server.cpp`
- `src/core/dispatcher.cpp`

### Web client (client/web)
- `client/web/src/vite-env.d.ts`
- `client/web/src/main.tsx`
- `client/web/src/types.ts`
- `client/web/src/styles/index.css`
- `client/web/src/web_interface.tsx`
- `client/web/src/App.tsx`
- `client/web/.env`, `client/web/.env.example`
- `client/web/postcss.config.js`
- `client/web/index.html`
- `client/web/vite.config.js`, `client/web/vite.config.ts`
- `client/web/tailwind.config.js`
- `client/web/tsconfig.json`, `client/web/tsconfig.node.json`
- `client/web/package.json`
- `client/web/.gitignore`

---

## 2) Ràng buộc an toàn & đạo đức (BẮT BUỘC)
Repo có file `keylogger.*`. Agent **không được**:
- Mở rộng tính năng theo hướng thu thập dữ liệu nhạy cảm trái phép, ẩn giấu, tự động gửi dữ liệu, hoặc bất kỳ hành vi xâm phạm quyền riêng tư.
- Thêm cơ chế stealth/persistence/ẩn tiến trình.

Agent **được**:
- Refactor để code rõ ràng hơn, an toàn hơn.
- **Vô hiệu hoá theo mặc định** các hành vi nhạy cảm.
- Bắt buộc **consent rõ ràng** (module `consent.*`) trước khi kích hoạt bất kỳ chức năng nhạy cảm.
- Thêm giới hạn/rate-limit, kiểm tra input, logging minh bạch, và cơ chế tắt.

Nếu phát hiện chức năng/luồng nào có rủi ro cao, ưu tiên:
- Đưa ra đề xuất an toàn hơn (ví dụ “remove/disable feature”, “require explicit opt-in UI + server-side check”).

---

## 3) Quy trình làm việc chuẩn (khuyến nghị)
### Bước A — Khảo sát & lập kế hoạch
1) Xác định cấu trúc: server/client C++, web client, luồng WebSocket, command dispatcher.
2) Liệt kê các lỗi tiềm năng:
   - Build/CMake/vcpkg (nếu có), include path, warning-as-error, mismatch standard.
   - Network WS: parse message, concurrency, disconnect handling, ping/pong, reconnection.
   - Module: system/screen/process/camera, quyền truy cập, lỗi OS-specific.
3) Ghi lại “Top issues” theo mức độ nghiêm trọng và cách tái hiện.

### Bước B — Làm cho dự án chạy được
- Ưu tiên làm **build chạy ổn** (C++ và web).
- Bổ sung hướng dẫn chạy trong README hoặc ghi chú ngắn (nếu repo có README) nhưng không bắt buộc.

### Bước C — Fix bug theo vòng lặp nhỏ
Mỗi bugfix:
1) Có bước tái hiện rõ (hoặc log/trace).
2) Fix tối thiểu, không “đập đi xây lại”.
3) Nếu dự án có test framework: thêm test.
4) Nếu chưa có test: thêm ít nhất kiểm tra runtime/validation/log giúp phát hiện regressions.

### Bước D — UI/UX facelift có hệ thống
- Dọn cấu trúc React: tách component, types rõ, state management gọn.
- Cải thiện trải nghiệm kết nối và thao tác.

---

## 4) Build/Run/Test — Quy tắc chung
### C++ (CMake)
- Ưu tiên dùng `CMakePresets.json` nếu có presets hợp lệ.
- Nếu không rõ lệnh:
  - Agent phải đọc `CMakeLists.txt` và `CMakePresets.json` để suy ra cách build đúng,
  - sau đó cập nhật phần “Build notes” trong commit message hoặc ghi chú.

Những việc bắt buộc khi sửa C++:
- Bật warning hợp lý (không phá build), giảm UB.
- Kiểm tra memory/resource leaks ở các luồng network/dispatcher.
- Chuẩn hoá error handling: trả về lỗi có mã/chuỗi rõ ràng.

### Web (Vite + React + Tailwind)
- Đọc `package.json` để xác định scripts (dev/build/lint/typecheck).
- Đảm bảo:
  - `npm install` / `pnpm install` chạy được,
  - `npm run dev` chạy,
  - `npm run build` không lỗi TypeScript,
  - (nếu có) `npm run lint` pass.

---

## 5) Tiêu chuẩn chất lượng code (C++)
- Không dùng `using namespace std;` trong header.
- Header include guard hoặc `#pragma once` nhất quán.
- Tránh include dư thừa, giảm coupling.
- Không để “magic string” cho command; nên centralize (enum/string constants).
- Tất cả message WebSocket phải có validation:
  - schema tối thiểu (type, payload),
  - size limit,
  - xử lý case thiếu field/sai type,
  - reject input nguy hiểm.

## 6) Tiêu chuẩn chất lượng code (Web)
- TypeScript strict-friendly (tránh `any`).
- Tách `types.ts` chuẩn: message types, API contracts.
- UI phải có:
  - Thanh trạng thái kết nối (Connected/Disconnected/Connecting).
  - Thông báo lỗi rõ ràng (toast/alert).
  - Empty-state + loading-state.
  - Layout responsive (mobile/desktop).
  - Chủ đề màu/typography nhất quán (Tailwind), spacing chuẩn.
  - Accessibility cơ bản: focus ring, aria-label cho nút quan trọng.

Gợi ý UI/UX hướng “modern”:
- App layout dạng: Sidebar (actions) + Main panel (logs/results) hoặc topbar + cards.
- Dùng “card”, “badge”, “chip”, “toast”, “modal confirm” (đặc biệt cho thao tác nguy hiểm).
- Dark mode (nếu dễ làm) và lưu preference.

---

## 7) Nguyên tắc riêng cho WebSocket & Command Dispatcher
- Chuẩn hoá format message (ví dụ):
  - `type`: string
  - `requestId`: string (để map request/response)
  - `payload`: object
  - `timestamp`: number (optional)
  - `error`: { code, message } (optional)

- Dispatcher:
  - Không crash khi nhận command lạ.
  - Command handler phải trả lỗi có cấu trúc.
  - Timeout cho request nếu có.
  - Logging có mức: info/warn/error.

- Consent gate:
  - Những command “nhạy cảm” (system_control/screen/camera/process/keylogger nếu còn tồn tại)
    phải check consent trước khi chạy.

---

## 8) Checklist “Done” (đầu ra bắt buộc)
### Bugfix
- [ ] Dự án build được (ít nhất 1 cấu hình hợp lệ).
- [ ] Fix các lỗi rõ ràng trong các file phạm vi.
- [ ] Không còn crash obvious khi kết nối/disconnect WS.
- [ ] Validation message vào/ra đầy đủ hơn trước.
- [ ] Logging/handling lỗi rõ ràng.

### UI/UX
- [ ] Giao diện hiện đại hơn, spacing/typography tốt, responsive.
- [ ] Có trạng thái kết nối + thông báo lỗi.
- [ ] Luồng thao tác rõ ràng, ít “mù mờ”.

### An toàn
- [ ] Các tính năng nhạy cảm bị vô hiệu hoá mặc định hoặc yêu cầu consent rõ ràng.
- [ ] Không thêm bất kỳ hành vi thu thập trái phép/ẩn giấu.

---

## 9) Cách bạn nên báo cáo kết quả
Khi hoàn thành, Agent phải tóm tắt:
1) Các bug đã tìm thấy + cách tái hiện + cách fix.
2) Những thay đổi UI/UX (kèm ảnh/chụp màn hình nếu có thể).
3) Những thay đổi liên quan an toàn/consent.
4) Các lệnh build/run đã xác nhận chạy được.

Kết thúc file.
