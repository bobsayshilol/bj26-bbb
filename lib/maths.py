import math

with open("maths.s", "w") as f:
    f.write(".section .data\n")
    f.write(".globl _sin_table\n")
    f.write(".globl _cos_table\n")
    f.write("_sin_table:\n")
    for i in range(256 + 256 // 4):
        if i == 256 // 4: # cos(x) = sin(x+pi/2) so save space by overlapping them
            f.write("_cos_table:\n")
        s = int(math.sin(2 * math.pi * i / 256) * 127)
        f.write(f"  .byte {s}\n")
