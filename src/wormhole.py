import math

pal_offset = 1
pal_size_x = 8
pal_size_y = 8

w = 256 # // 2
h = 224 # // 2

debug = False

def F(x,y,z):
    # z = 1 - 1 / (x^2 + y^2)
    return z - (1 - 1 / (x**2 + y**2))

cx,cy,cz = 0,-3,3 # camera
look_angle = math.pi / 3 # 0 = straight down
dx,dy,dz = 0,math.sin(look_angle),-math.cos(look_angle) # looking dir
fov_angle = math.pi / 2
fov = math.tan(fov_angle / 2) * 2

# Ray trace given x,y in screen space (0-1).
def trace(x,y):
    sx,sy,sz = x*fov+dx,y*fov+dy,dz # step dir
    t = 0
    while True:
        px,py,pz = cx+sx*t,cy+sy*t,cz+sz*t
        t += 1e-2
        if F(px,py,pz) < 0: # crossed the surface
            break
    return px,py,pz

# Depth for debuggin.
def depth(px,py,pz):
    lx,ly,lz = px-cx,py-cy,pz-cz
    return math.sqrt(lx**2 + ly**2 + lz**2)

# Convert to a palette.
def to_pal(px,py,pz):
    z_offset = 4
    x_scale = 2 # bodge
    r = px**2 + py**2 + (pz - z_offset)**2
    pal_x = int(r * x_scale)

    pal_y_sections = 16
    pal_y = int(pal_y_sections * pal_size_y * math.atan2(py,px) / (2 * math.pi))

    return 8 * (pal_y % pal_size_y) + (pal_x % pal_size_x)

if debug:
    from PIL import Image
    img_d = Image.new(mode="L", size=(w,h))
    img_pal = Image.new(mode="L", size=(w,h))

print("Generating. This takes a while...")
output = []
for y in range(h):
    if (y & 7) == 0:
        print(f"Line {y}/{h}")
    for x in range(w):
        px,py,pz = trace(x/w-0.5,0.5-y/h)
        pal = to_pal(px,py,pz)
        output.append(pal)
        if debug:
            d = depth(px,py,pz)
            img_d.putpixel((x,y), int(d * 50))
            img_pal.putpixel((x,y), pal)

print("Compressing")

# Delta compress.
start = output[0]
deltas = []
for i in range(len(output) - 1):
    n = output[i + 1]
    o = output[i]
    d = n - o
    # Map to range [-32, 31]. Assumes palette size is 64.
    if d < -32: d += 64
    if d >= 32: d -= 64
    # Zigzag. Range is now [0, 63]
    d = (-2 * d - 1) if d < 0 else (2 * d)
    assert(d >= 0)
    assert(d < 64)
    deltas.append(d)

# Chunk into blocks.
compressed = []
idx = 0
while idx < len(deltas):
    # Z Z|Y Y|X X|0 0
    # Y Y|X X X X|0 1
    # Y|X X X X X|1 0
    # X X X X X X|1 1

    d0 = deltas[idx]
    d1 = deltas[idx + 1] if idx + 1 < len(deltas) else 1000
    d2 = deltas[idx + 2] if idx + 2 < len(deltas) else 1000

    if d0 < 4 and d1 < 4 and d2 < 4:
        d = 0 | (d0 << 2) | (d1 << 4) | (d2 << 6)
        compressed.append(d)
        idx += 3
    elif d0 < 16 and d1 < 4:
        d = 1 | (d0 << 2) | (d1 << 6)
        compressed.append(d)
        idx += 2
    elif d0 < 32 and d1 < 2:
        d = 2 | (d0 << 2) | (d1 << 7)
        compressed.append(d)
        idx += 2
    elif d0 < 64:
        d = 3 | (d0 << 2)
        compressed.append(d)
        idx += 1
    else:
        assert(False)

print("Writing it out")
with open("data_wormhole.cc", "w") as f:
    f.write("#include \"images.h\"\n")
    f.write("namespace game::images {\n")
    f.write(f"static_assert(wormhole::pal_size == {pal_size_x * pal_size_y});\n")
    f.write(f"static_assert(wormhole::width == {w});\n")
    f.write(f"static_assert(wormhole::height == {h});\n")
    f.write(f"static constexpr uint8_t start = {start};\n")
    f.write("static constexpr uint8_t compressed[] {\n")
    i = 0
    for val in compressed:
        f.write(f"{val}, ")
        i += 1
        if (i & 63) == 63:
            f.write("\n")
    f.write("};\n")
    f.write("""
void wormhole::decompress(uint8_t * output) {
    [[maybe_unused]] auto * begin = output;

    uint8_t acc = start;
    *output++ = acc;

    auto emit = [&](uint8_t val) {
        // https://lemire.me/blog/2022/11/25/making-all-your-integers-positive-with-zigzag-encoding/
        uint8_t d = (val >> 1) ^ (-(val&1));
        acc += d;
        acc &= 63;
        *output++ = pal_offset + acc;
    };

    for (uint8_t value : compressed) {
        const uint8_t type = value & 3;
        switch (type) {
            case 0:
                emit((value >> 2) & 3);
                emit((value >> 4) & 3);
                emit((value >> 6) & 3);
                break;
            case 1:
                emit((value >> 2) & 15);
                emit((value >> 6) & 3);
                break;
            case 2:
                emit((value >> 2) & 31);
                emit((value >> 7) & 1);
                break;
            case 3:
                emit((value >> 2) & 63);
                break;
        }
    }
    ASSERT(output == begin + width * height);
}
""")
    f.write("} // namespace game::images\n")

    if debug:
        img_d.show()
        img_pal.show()
