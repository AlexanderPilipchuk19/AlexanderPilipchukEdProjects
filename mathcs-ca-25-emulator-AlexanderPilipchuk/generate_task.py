import struct

def encode_r_type(funct7, rs2, rs1, funct3, rd, opcode):
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

def encode_i_type(imm, rs1, funct3, rd, opcode):
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

def encode_s_type(imm, rs2, rs1, funct3, opcode):
    imm_11_5 = (imm >> 5) & 0x7F
    imm_4_0 = imm & 0x1F
    return (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_0 << 7) | opcode

def encode_b_type(imm, rs2, rs1, funct3, opcode):
    imm_12 = (imm >> 12) & 0x1
    imm_10_5 = (imm >> 5) & 0x3F
    imm_4_1 = (imm >> 1) & 0xF
    imm_11 = (imm >> 11) & 0x1
    return (imm_12 << 31) | (imm_10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_1 << 8) | (imm_11 << 7) | opcode

def encode_u_type(imm, rd, opcode):
    return (imm & 0xFFFFF000) | (rd << 7) | opcode

def encode_j_type(imm, rd, opcode):
    imm_20 = (imm >> 20) & 0x1
    imm_10_1 = (imm >> 1) & 0x3FF
    imm_11 = (imm >> 11) & 0x1
    imm_19_12 = (imm >> 12) & 0xFF
    return (imm_20 << 31) | (imm_19_12 << 12) | (imm_11 << 20) | (imm_10_1 << 21) | (rd << 7) | opcode

instructions = []

instructions.append(encode_u_type(0x1 << 12, 6, 0x37))
instructions.append(encode_i_type(0x200, 6, 0, 6, 0x13))

instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))

instructions.append(encode_i_type(-0x800 & 0xFFF, 6, 0, 6, 0x13))
instructions.append(encode_i_type(-0x400 & 0xFFF, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0x400, 6, 0, 6, 0x13))
instructions.append(encode_i_type(0, 6, 2, 7, 0x03))

instructions.append(encode_i_type(21, 0, 0, 5, 0x13))
loop_idx = len(instructions)
instructions.append(encode_i_type(-1 & 0xFFF, 5, 0, 5, 0x13))
bne_idx = len(instructions)
instructions.append(0)
instructions.append(encode_i_type(0, 1, 0, 0, 0x67))

bne_imm = (loop_idx - bne_idx) * 4
instructions[bne_idx] = encode_b_type(bne_imm, 0, 5, 1, 0x63)

code_start = 0x20000
pc = code_start
ra = 0x0

registers = [0] * 31
registers[0] = ra

with open('task.bin', 'wb') as f:
    f.write(struct.pack('<I', pc))
    for reg in registers:
        f.write(struct.pack('<I', reg))
    
    code_size = len(instructions) * 4
    f.write(struct.pack('<I', code_start))
    f.write(struct.pack('<I', code_size))
    for instr in instructions:
        f.write(struct.pack('<I', instr))
    
    data_start = 0x1000
    data_size = 0x4000
    f.write(struct.pack('<I', data_start))
    f.write(struct.pack('<I', data_size))
    f.write(bytes(data_size))

print(f"Generated task.bin with {len(instructions)} instructions")
print(f"Code at 0x{code_start:x}, size {code_size} bytes")

