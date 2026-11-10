# This function is never invoked. Its code section marks the package as a
# code mod, so DinoTale loads the companion native library before game boot
# and requires a game restart when the mod is enabled or disabled.
.set noreorder
.text
.globl dinotale32x9_bootstrap
.type dinotale32x9_bootstrap, @function
dinotale32x9_bootstrap:
    jr $ra
    nop
.size dinotale32x9_bootstrap, .-dinotale32x9_bootstrap
