# Licensing scope

This is a review snapshot of KotOR:CE, built on K12. It does not claim ownership
of third-party code, proprietary games or game assets.

The K12 foundation is supplied by its authors under CC0 1.0; the original CC0 text in LICENSES/K12-CC0-1.0.txt
and existing notices are preserved. OpenJK-derived code is GPL-2.0-only, so the
combined engine must be distributed in compliance with GPL version 2, including
its complete corresponding source and necessary build scripts. Original file
licenses remain applicable. The K12 CC0 dedication is not a license for the entire
combined engine or any proprietary asset.

For this Windows build, the selected alternatives are FreeType GPLv2, HIDAPI
BSD-3-Clause, miniaudio MIT-0 and unbikit MIT. Authentic selected notices are in
LICENSES. Existing dual-license notices contained in the vendored K12 foundation
are preserved in their original source locations; they are not additional
license claims for unrelated assets.

The LZMA decoder/header files actually used declare public-domain status. No
7-Zip RAR code, DotZLib, SDL test/demo assets or unbikit logos are added. Their
unrelated notices from the earlier broad runtime-notice copy are omitted.

KMRP font data is not part of this source snapshot. Its authentic OFL notice is
retained for the held data package that uses those fonts. Code licenses do not
license KotOR or Jedi Academy models, animations, textures, sounds or saves.
Upstream icons/shaders/glyph resources retained with K12 require the provenance
review recorded in the local publication review before a public release.

Microsoft Windows libraries and the MSVC runtime are governed by Microsoft's
terms, not these open-source licenses. Confirm compiler/runtime redistribution
rights before publishing compiled artifacts; having a DLL locally is not proof
of redistribution permission. No private symbols, personal runtime settings or
old Git history are needed in a public source distribution.

The Windows privacy build disables unused SDL OpenGL ES/EGL and offscreen video
drivers. Desktop OpenGL remains enabled. This avoids carrying an unused EGL
header path with Apache-2.0-only declarations into a GPLv2-only combination.
The desktop OpenGL Mesa/Khronos MIT notices are retained for compiled headers.

The same Windows-only build omits unused SDL Vulkan support. The corresponding
source archive excludes the unused Apache-2.0 EGL/Vulkan API headers after
verifying the configured build does not include them. It retains all source and
notices required by the selected desktop OpenGL path.
