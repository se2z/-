# Gates: restore previous panel and keep current features

OWNS: src/gui.cpp, dist/notify_bot.exe

Scope: keep the previous dark pink Notify panel while exposing the current Bot API notification workflow in the client executable.

- [ ] G1: current C++20 Release targets build
  CHECK: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release --parallel && echo BUILD_OK
  EXPECT: BUILD_OK
  EVIDENCE: pending

- [ ] G2: pure tests pass
  CHECK: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -C Release --output-on-failure && echo TESTS_OK
  EXPECT: TESTS_OK
  EVIDENCE: pending

- [ ] G3: release GUI visually retains previous Notify identity and exposes publishing controls
  EVIDENCE: pending
