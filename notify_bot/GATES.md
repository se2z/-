# Release readiness gates

- [x] G1: the x64 Release configuration builds all declared targets
  CHECK: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build_release --config Release --parallel && echo BUILD_OK
  EXPECT: BUILD_OK
  EVIDENCE: automatic-evidence=v1; definition-sha256=995d5356c1d3f8006ccda9853f0757e70c81608c416c9d016f2300be43262d2b; exit=0; EXPECT=matched; output-sha256=80aec8c5b55f2ad8fb2bc5fb8f342d573865cd351be0e3d4265c1d1325da3c89; output-bytes=665; shell=C:\Windows\system32\cmd.exe; cwd=C:\да\все проекты\заказ 1\notify_bot; path=a1c22db694dd/34 entries

- [x] G2: pure tests pass against the Release build
  CHECK: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build_release -C Release --output-on-failure && echo TESTS_OK
  EXPECT: TESTS_OK
  EVIDENCE: automatic-evidence=v1; definition-sha256=fa8fd8cac1f509766409e3535efdab02cf7d9710b07a35ed7c39fdde8a53d320; exit=0; EXPECT=matched; output-sha256=de5b3271e751f7bf2e5d06408d0ca4ca819a3a79a88d75bf3adc6161f7bb8db4; output-bytes=367; shell=C:\Windows\system32\cmd.exe; cwd=C:\да\все проекты\заказ 1\notify_bot; path=a1c22db694dd/34 entries

- [x] G3: the client release folder contains only the standalone executable and it matches the built client
  CHECK: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -E compare_files build_release/Release/notify_bot.exe dist/notify_bot.exe && echo release artifact verified
  EXPECT: release artifact verified
  EVIDENCE: automatic-evidence=v1; definition-sha256=e9c573304d03127b24ed1e5854daa3b8876173cf2d89bf2dfa96212043147e2d; exit=0; EXPECT=matched; output-sha256=f29a491b40b024e9cc11e6c5ff9d836cc685cb3a6e469fee5773c20a84840935; output-bytes=27; shell=C:\Windows\system32\cmd.exe; cwd=C:\да\все проекты\заказ 1\notify_bot; path=a1c22db694dd/34 entries
