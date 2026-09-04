[app]
title = NC Terminal
package.name = ncterminal
package.domain = com.nwlsystems
source.dir = .
source.include_exts = py,so
version = 1.0

requirements = python3,kivy,pyjnius

orientation = portrait
fullscreen = 0

# .so pre-compilados (gerados por build_native_libs.sh) - viram binarios
# executaveis reais dentro do app depois de instalado.
android.add_libs_arm64_v8a = libs/arm64-v8a/*.so
android.add_libs_armeabi_v7a = libs/armeabi-v7a/*.so

android.api = 33
android.minapi = 24
android.ndk_api = 24
android.archs = arm64-v8a, armeabi-v7a
android.allow_backup = True

[buildozer]
log_level = 2
warn_on_root = 1
