#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

constexpr uint32_t MEMORY_SIZE = 256 * 1024;
constexpr uint32_t ADDRESS_LEN = 18;
constexpr uint32_t CACHE_TAG_LEN = 8;
constexpr uint32_t CACHE_INDEX_LEN = 5;
constexpr uint32_t CACHE_OFFSET_LEN = 5;
constexpr uint32_t CACHE_SET_COUNT = 32;
constexpr uint32_t CACHE_WAY = 4;
constexpr uint32_t CACHE_LINE_SIZE = 32;
constexpr uint32_t CACHE_LINE_COUNT = 128;
constexpr uint32_t CACHE_SIZE = 4096;

struct MemoryFragment {
    uint32_t address;
    std::vector<uint8_t> data;
};

struct InputData {
    uint32_t pc;
    uint32_t registers[31];
    std::vector<MemoryFragment> fragments;
};

static uint32_t readUint32LE(std::ifstream& file) {
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    if (!file) throw std::runtime_error("Failed to read from input file");
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

static void writeUint32LE(std::ofstream& file, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    file.write(reinterpret_cast<const char*>(bytes), 4);
    if (!file) throw std::runtime_error("Failed to write to output file");
}

static InputData readInputFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open input file: " + filename);

    InputData data;
    data.pc = readUint32LE(file);
    for (int i = 0; i < 31; i++) data.registers[i] = readUint32LE(file);

    while (file.peek() != EOF) {
        MemoryFragment fragment;
        fragment.address = readUint32LE(file);
        uint32_t size = readUint32LE(file);
        fragment.data.resize(size);
        file.read(reinterpret_cast<char*>(fragment.data.data()), size);
        if (!file && !file.eof()) throw std::runtime_error("Failed to read memory fragment");
        data.fragments.push_back(fragment);
        if (file.eof()) break;
    }
    return data;
}

static void writeOutputFile(const std::string& filename, uint32_t pc, const uint32_t registers[31],
                            uint32_t address, uint32_t size, const uint8_t* memory) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open output file: " + filename);
    writeUint32LE(file, pc);
    for (int i = 0; i < 31; i++) writeUint32LE(file, registers[i]);
    writeUint32LE(file, address);
    writeUint32LE(file, size);
    file.write(reinterpret_cast<const char*>(memory + address), size);
    if (!file) throw std::runtime_error("Failed to write output file");
}

static uint32_t parseNumber(const std::string& str) {
    try {
        size_t pos;
        uint32_t value;
        if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) value = std::stoul(str, &pos, 16);
        else value = std::stoul(str, &pos, 10);
        if (pos != str.size()) throw std::invalid_argument("Invalid number format");
        return value;
    } catch (...) {
        throw std::runtime_error("Invalid number: " + str);
    }
}

class Memory {
private:
    uint8_t data[MEMORY_SIZE];
    bool initialized[MEMORY_SIZE];
public:
    Memory() {
        std::memset(data, 0, sizeof(data));
        std::memset(initialized, 0, sizeof(initialized));
    }
    void loadFragments(const std::vector<MemoryFragment>& fragments) {
        for (const auto& fragment : fragments) {
            if (fragment.address + fragment.data.size() > MEMORY_SIZE)
                throw std::runtime_error("Memory fragment out of bounds");
            std::memcpy(data + fragment.address, fragment.data.data(), fragment.data.size());
            std::memset(initialized + fragment.address, 1, fragment.data.size());
        }
    }
    uint8_t read8(uint32_t address) {
        if (address >= MEMORY_SIZE) throw std::runtime_error("Memory read out of bounds");
        return data[address];
    }
    uint16_t read16(uint32_t address) {
        if (address + 1 >= MEMORY_SIZE) throw std::runtime_error("Memory read out of bounds");
        return static_cast<uint16_t>(data[address]) |
               (static_cast<uint16_t>(data[address + 1]) << 8);
    }
    uint32_t read32(uint32_t address) {
        if (address + 3 >= MEMORY_SIZE) throw std::runtime_error("Memory read out of bounds");
        return static_cast<uint32_t>(data[address]) |
               (static_cast<uint32_t>(data[address + 1]) << 8) |
               (static_cast<uint32_t>(data[address + 2]) << 16) |
               (static_cast<uint32_t>(data[address + 3]) << 24);
    }
    void write8(uint32_t address, uint8_t value) {
        if (address >= MEMORY_SIZE) throw std::runtime_error("Memory write out of bounds");
        data[address] = value;
        initialized[address] = true;
    }
    void write16(uint32_t address, uint16_t value) {
        if (address + 1 >= MEMORY_SIZE) throw std::runtime_error("Memory write out of bounds");
        data[address] = value & 0xFF;
        data[address + 1] = (value >> 8) & 0xFF;
        initialized[address] = true;
        initialized[address + 1] = true;
    }
    void write32(uint32_t address, uint32_t value) {
        if (address + 3 >= MEMORY_SIZE) throw std::runtime_error("Memory write out of bounds");
        data[address] = value & 0xFF;
        data[address + 1] = (value >> 8) & 0xFF;
        data[address + 2] = (value >> 16) & 0xFF;
        data[address + 3] = (value >> 24) & 0xFF;
        initialized[address] = true;
        initialized[address + 1] = true;
        initialized[address + 2] = true;
        initialized[address + 3] = true;
    }
    const uint8_t* getData() const { return data; }
};

class CPU {
private:
    uint32_t pc;
    uint32_t regs[32];
public:
    CPU() : pc(0) { std::memset(regs, 0, sizeof(regs)); }
    void loadState(uint32_t initial_pc, const uint32_t initial_regs[31]) {
        pc = initial_pc;
        regs[0] = 0;
        for (int i = 1; i < 32; i++) regs[i] = initial_regs[i - 1];
    }
    void saveState(uint32_t& out_pc, uint32_t out_regs[31]) const {
        out_pc = pc;
        for (int i = 1; i < 32; i++) out_regs[i - 1] = regs[i];
    }
    uint32_t getPC() const { return pc; }
    void setPC(uint32_t value) { pc = value; }
    uint32_t getReg(uint32_t index) const {
        if (index >= 32) throw std::runtime_error("Invalid register index");
        return (index == 0) ? 0 : regs[index];
    }
    void setReg(uint32_t index, uint32_t value) {
        if (index >= 32) throw std::runtime_error("Invalid register index");
        if (index != 0) regs[index] = value;
    }
};

enum InstructionType {
    INST_ADD, INST_SUB, INST_ADDI,
    INST_SLL, INST_SRL, INST_SRA, INST_SLLI, INST_SRLI, INST_SRAI,
    INST_AND, INST_OR, INST_XOR, INST_ANDI, INST_ORI, INST_XORI,
    INST_SLT, INST_SLTU, INST_SLTI, INST_SLTIU,
    INST_LUI, INST_AUIPC,
    INST_LB, INST_LH, INST_LW, INST_LBU, INST_LHU,
    INST_SB, INST_SH, INST_SW,
    INST_BEQ, INST_BNE, INST_BLT, INST_BGE, INST_BLTU, INST_BGEU,
    INST_JAL, INST_JALR,
    INST_ECALL, INST_EBREAK,
    INST_MUL, INST_MULH, INST_MULHSU, INST_MULHU,
    INST_DIV, INST_DIVU, INST_REM, INST_REMU,
    INST_UNKNOWN
};

struct DecodedInstruction {
    InstructionType type;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    int32_t imm;
};

class Decoder {
private:
    int32_t signExtend(uint32_t value, int bits) {
        uint32_t sign_bit = 1u << (bits - 1);
        if (value & sign_bit) return static_cast<int32_t>(value | (~0u << bits));
        return static_cast<int32_t>(value);
    }
public:
    DecodedInstruction decode(uint32_t instruction) {
        DecodedInstruction decoded;
        decoded.type = INST_UNKNOWN;
        decoded.rd = 0;
        decoded.rs1 = 0;
        decoded.rs2 = 0;
        decoded.imm = 0;

        uint32_t opcode = instruction & 0x7F;
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct7 = (instruction >> 25) & 0x7F;

        decoded.rd = rd;
        decoded.rs1 = rs1;
        decoded.rs2 = rs2;

        switch (opcode) {
            case 0x33: {
                if (funct7 == 0x00) {
                    if (funct3 == 0x0) decoded.type = INST_ADD;
                    else if (funct3 == 0x1) decoded.type = INST_SLL;
                    else if (funct3 == 0x2) decoded.type = INST_SLT;
                    else if (funct3 == 0x3) decoded.type = INST_SLTU;
                    else if (funct3 == 0x4) decoded.type = INST_XOR;
                    else if (funct3 == 0x5) decoded.type = INST_SRL;
                    else if (funct3 == 0x6) decoded.type = INST_OR;
                    else if (funct3 == 0x7) decoded.type = INST_AND;
                } else if (funct7 == 0x20) {
                    if (funct3 == 0x0) decoded.type = INST_SUB;
                    else if (funct3 == 0x5) decoded.type = INST_SRA;
                } else if (funct7 == 0x01) {
                    if (funct3 == 0x0) decoded.type = INST_MUL;
                    else if (funct3 == 0x1) decoded.type = INST_MULH;
                    else if (funct3 == 0x2) decoded.type = INST_MULHSU;
                    else if (funct3 == 0x3) decoded.type = INST_MULHU;
                    else if (funct3 == 0x4) decoded.type = INST_DIV;
                    else if (funct3 == 0x5) decoded.type = INST_DIVU;
                    else if (funct3 == 0x6) decoded.type = INST_REM;
                    else if (funct3 == 0x7) decoded.type = INST_REMU;
                }
                break;
            }
            case 0x13: {
                uint32_t imm_i = instruction >> 20;
                decoded.imm = signExtend(imm_i, 12);
                if (funct3 == 0x0) decoded.type = INST_ADDI;
                else if (funct3 == 0x1 && funct7 == 0x00) decoded.type = INST_SLLI;
                else if (funct3 == 0x2) decoded.type = INST_SLTI;
                else if (funct3 == 0x3) decoded.type = INST_SLTIU;
                else if (funct3 == 0x4) decoded.type = INST_XORI;
                else if (funct3 == 0x5 && funct7 == 0x00) decoded.type = INST_SRLI;
                else if (funct3 == 0x5 && funct7 == 0x20) decoded.type = INST_SRAI;
                else if (funct3 == 0x6) decoded.type = INST_ORI;
                else if (funct3 == 0x7) decoded.type = INST_ANDI;
                break;
            }
            case 0x03: {
                uint32_t imm_i = instruction >> 20;
                decoded.imm = signExtend(imm_i, 12);
                if (funct3 == 0x0) decoded.type = INST_LB;
                else if (funct3 == 0x1) decoded.type = INST_LH;
                else if (funct3 == 0x2) decoded.type = INST_LW;
                else if (funct3 == 0x4) decoded.type = INST_LBU;
                else if (funct3 == 0x5) decoded.type = INST_LHU;
                break;
            }
            case 0x23: {
                uint32_t imm_s = ((instruction >> 7) & 0x1F) | ((instruction >> 20) & 0xFE0);
                decoded.imm = signExtend(imm_s, 12);
                if (funct3 == 0x0) decoded.type = INST_SB;
                else if (funct3 == 0x1) decoded.type = INST_SH;
                else if (funct3 == 0x2) decoded.type = INST_SW;
                break;
            }
            case 0x63: {
                uint32_t imm_b = ((instruction >> 7) & 0x1E) |
                                 ((instruction >> 20) & 0x7E0) |
                                 ((instruction << 4) & 0x800) |
                                 ((instruction >> 19) & 0x1000);
                decoded.imm = signExtend(imm_b, 13);
                if (funct3 == 0x0) decoded.type = INST_BEQ;
                else if (funct3 == 0x1) decoded.type = INST_BNE;
                else if (funct3 == 0x4) decoded.type = INST_BLT;
                else if (funct3 == 0x5) decoded.type = INST_BGE;
                else if (funct3 == 0x6) decoded.type = INST_BLTU;
                else if (funct3 == 0x7) decoded.type = INST_BGEU;
                break;
            }
            case 0x37:
                decoded.imm = instruction & 0xFFFFF000;
                decoded.type = INST_LUI;
                break;
            case 0x17:
                decoded.imm = instruction & 0xFFFFF000;
                decoded.type = INST_AUIPC;
                break;
            case 0x6F: {
                uint32_t imm_j = ((instruction >> 20) & 0x7FE) |
                                 ((instruction >> 9) & 0x800) |
                                 (instruction & 0xFF000) |
                                 ((instruction >> 11) & 0x100000);
                decoded.imm = signExtend(imm_j, 21);
                decoded.type = INST_JAL;
                break;
            }
            case 0x67: {
                uint32_t imm_i = instruction >> 20;
                decoded.imm = signExtend(imm_i, 12);
                if (funct3 == 0x0) decoded.type = INST_JALR;
                break;
            }
            case 0x73: {
                if (funct3 == 0x0) {
                    uint32_t imm_i = instruction >> 20;
                    if (imm_i == 0) decoded.type = INST_ECALL;
                    else if (imm_i == 1) decoded.type = INST_EBREAK;
                }
                break;
            }
        }
        return decoded;
    }
};

struct CacheLine {
    bool valid;
    bool dirty;
    uint32_t tag;
    uint8_t data[CACHE_LINE_SIZE];
    bool maxused;
};

struct CacheStats {
    uint64_t instr_access;
    uint64_t instr_hit;
    uint64_t data_access;
    uint64_t data_hit;
    CacheStats() : instr_access(0), instr_hit(0), data_access(0), data_hit(0) {}
};

class Cache {
protected:
    CacheLine lines[CACHE_SET_COUNT][CACHE_WAY];
    Memory* memory;
    CacheStats stats;
    struct AddressParts { uint32_t tag; uint32_t index; uint32_t offset; };
    AddressParts parseAddress(uint32_t address) {
        AddressParts parts;
        parts.offset = address & ((1u << CACHE_OFFSET_LEN) - 1);
        parts.index = (address >> CACHE_OFFSET_LEN) & ((1u << CACHE_INDEX_LEN) - 1);
        parts.tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN)) & ((1u << CACHE_TAG_LEN) - 1);
        return parts;
    }
    void loadLine(uint32_t set_index, uint32_t way, uint32_t address) {
        uint32_t line_address = address & ~(CACHE_LINE_SIZE - 1);
        for (uint32_t i = 0; i < CACHE_LINE_SIZE; i++)
            lines[set_index][way].data[i] = memory->read8(line_address + i);
        lines[set_index][way].tag = parseAddress(address).tag;
        lines[set_index][way].valid = true;
        lines[set_index][way].dirty = false;
    }
    void evictLine(uint32_t set_index, uint32_t way) {
        if (lines[set_index][way].valid && lines[set_index][way].dirty) {
            uint32_t address = (lines[set_index][way].tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN)) |
                               (set_index << CACHE_OFFSET_LEN);
            for (uint32_t i = 0; i < CACHE_LINE_SIZE; i++)
                memory->write8(address + i, lines[set_index][way].data[i]);
        }
    }
    virtual uint32_t selectVictim(uint32_t set_index) = 0;
    virtual void updateReplacement(uint32_t set_index, uint32_t way) = 0;
public:
    Cache(Memory* mem) : memory(mem) {
        for (uint32_t i = 0; i < CACHE_SET_COUNT; i++) {
            for (uint32_t j = 0; j < CACHE_WAY; j++) {
                lines[i][j].valid = false;
                lines[i][j].dirty = false;
                lines[i][j].tag = 0;
                lines[i][j].maxused = false;
                std::memset(lines[i][j].data, 0, CACHE_LINE_SIZE);
            }
        }
    }
    virtual ~Cache() = default;
    uint32_t read(uint32_t address, uint32_t size, bool is_instruction) {
        AddressParts parts = parseAddress(address);
        if (is_instruction) stats.instr_access++;
        else stats.data_access++;
        int hit_way = -1;
        for (uint32_t way = 0; way < CACHE_WAY; way++) {
            if (lines[parts.index][way].valid && lines[parts.index][way].tag == parts.tag) {
                hit_way = static_cast<int>(way);
                break;
            }
        }
        if (hit_way >= 0) {
            if (is_instruction) stats.instr_hit++;
            else stats.data_hit++;
            updateReplacement(parts.index, static_cast<uint32_t>(hit_way));
        } else {
            uint32_t victim = selectVictim(parts.index);
            evictLine(parts.index, victim);
            loadLine(parts.index, victim, address);
            updateReplacement(parts.index, victim);
            hit_way = static_cast<int>(victim);
        }
        uint32_t result = 0;
        for (uint32_t i = 0; i < size; i++)
            result |= static_cast<uint32_t>(lines[parts.index][hit_way].data[parts.offset + i]) << (i * 8);
        return result;
    }
    void write(uint32_t address, uint32_t value, uint32_t size) {
        AddressParts parts = parseAddress(address);
        stats.data_access++;
        int hit_way = -1;
        for (uint32_t way = 0; way < CACHE_WAY; way++) {
            if (lines[parts.index][way].valid && lines[parts.index][way].tag == parts.tag) {
                hit_way = static_cast<int>(way);
                break;
            }
        }
        if (hit_way >= 0) {
            stats.data_hit++;
            updateReplacement(parts.index, static_cast<uint32_t>(hit_way));
        } else {
            uint32_t victim = selectVictim(parts.index);
            evictLine(parts.index, victim);
            loadLine(parts.index, victim, address);
            updateReplacement(parts.index, victim);
            hit_way = static_cast<int>(victim);
        }
        for (uint32_t i = 0; i < size; i++)
            lines[parts.index][hit_way].data[parts.offset + i] = (value >> (i * 8)) & 0xFF;
        lines[parts.index][hit_way].dirty = true;
    }
    void flush() {
        for (uint32_t i = 0; i < CACHE_SET_COUNT; i++)
            for (uint32_t j = 0; j < CACHE_WAY; j++)
                evictLine(i, j);
    }
    const CacheStats& getStats() const { return stats; }
};

class CacheLRU : public Cache {
private:
    uint64_t access_time[CACHE_SET_COUNT][CACHE_WAY];
    uint64_t global_time;
protected:
    uint32_t selectVictim(uint32_t set_index) override {
        uint32_t victim = 0;
        uint64_t min_time = access_time[set_index][0];
        for (uint32_t way = 1; way < CACHE_WAY; way++) {
            if (!lines[set_index][way].valid) return way;
            if (access_time[set_index][way] < min_time) {
                min_time = access_time[set_index][way];
                victim = way;
            }
        }
        return victim;
    }
    void updateReplacement(uint32_t set_index, uint32_t way) override {
        access_time[set_index][way] = global_time++;
    }
public:
    CacheLRU(Memory* mem) : Cache(mem), global_time(0) {
        std::memset(access_time, 0, sizeof(access_time));
    }
};

class CacheBitPseudoLRU : public Cache {
protected:
    uint32_t selectVictim(uint32_t set_index) override {
        for (uint32_t way = 0; way < CACHE_WAY; ++way) {
            if (!lines[set_index][way].valid) {
                return way;
            }
        }

        for (uint32_t way = 0; way < CACHE_WAY; ++way) {
            if (!lines[set_index][way].maxused) {
                return way;
            }
        }

        return 0;
    }

    void updateReplacement(uint32_t set_index, uint32_t way) override {
        lines[set_index][way].maxused = true;

        bool all_used = true;
        for (uint32_t w = 0; w < CACHE_WAY; ++w) {
            if (!lines[set_index][w].maxused) {
                all_used = false;
                break;
            }
        }

        if (all_used) {
            for (uint32_t w = 0; w < CACHE_WAY; ++w) {
                lines[set_index][w].maxused = false;
            }
            lines[set_index][way].maxused = true;
        }
    }

public:
    CacheBitPseudoLRU(Memory* mem) : Cache(mem) {}
};

class Executor {
public:
    static bool execute(const DecodedInstruction& inst, CPU& cpu, Memory& memory) {
        static_cast<void>(memory);
        uint32_t pc = cpu.getPC();
        uint32_t rs1_val = cpu.getReg(inst.rs1);
        uint32_t rs2_val = cpu.getReg(inst.rs2);
        switch (inst.type) {
            case INST_ADD:
                cpu.setReg(inst.rd, rs1_val + rs2_val);
                cpu.setPC(pc + 4);
                break;
            case INST_SUB:
                cpu.setReg(inst.rd, rs1_val - rs2_val);
                cpu.setPC(pc + 4);
                break;
            case INST_ADDI:
                cpu.setReg(inst.rd, rs1_val + inst.imm);
                cpu.setPC(pc + 4);
                break;
            case INST_SLL:
                cpu.setReg(inst.rd, rs1_val << (rs2_val & 0x1F));
                cpu.setPC(pc + 4);
                break;
            case INST_SRL:
                cpu.setReg(inst.rd, rs1_val >> (rs2_val & 0x1F));
                cpu.setPC(pc + 4);
                break;
            case INST_SRA:
                cpu.setReg(inst.rd, static_cast<uint32_t>(static_cast<int32_t>(rs1_val) >> (rs2_val & 0x1F)));
                cpu.setPC(pc + 4);
                break;
            case INST_SLLI:
                cpu.setReg(inst.rd, rs1_val << (inst.imm & 0x1F));
                cpu.setPC(pc + 4);
                break;
            case INST_SRLI:
                cpu.setReg(inst.rd, rs1_val >> (inst.imm & 0x1F));
                cpu.setPC(pc + 4);
                break;
            case INST_SRAI:
                cpu.setReg(inst.rd, static_cast<uint32_t>(static_cast<int32_t>(rs1_val) >> (inst.imm & 0x1F)));
                cpu.setPC(pc + 4);
                break;
            case INST_AND:
                cpu.setReg(inst.rd, rs1_val & rs2_val);
                cpu.setPC(pc + 4);
                break;
            case INST_OR:
                cpu.setReg(inst.rd, rs1_val | rs2_val);
                cpu.setPC(pc + 4);
                break;
            case INST_XOR:
                cpu.setReg(inst.rd, rs1_val ^ rs2_val);
                cpu.setPC(pc + 4);
                break;
            case INST_ANDI:
                cpu.setReg(inst.rd, rs1_val & inst.imm);
                cpu.setPC(pc + 4);
                break;
            case INST_ORI:
                cpu.setReg(inst.rd, rs1_val | inst.imm);
                cpu.setPC(pc + 4);
                break;
            case INST_XORI:
                cpu.setReg(inst.rd, rs1_val ^ inst.imm);
                cpu.setPC(pc + 4);
                break;
            case INST_SLT:
                cpu.setReg(inst.rd, (static_cast<int32_t>(rs1_val) < static_cast<int32_t>(rs2_val)) ? 1u : 0u);
                cpu.setPC(pc + 4);
                break;
            case INST_SLTU:
                cpu.setReg(inst.rd, (rs1_val < rs2_val) ? 1u : 0u);
                cpu.setPC(pc + 4);
                break;
            case INST_SLTI:
                cpu.setReg(inst.rd, (static_cast<int32_t>(rs1_val) < inst.imm) ? 1u : 0u);
                cpu.setPC(pc + 4);
                break;
            case INST_SLTIU:
                cpu.setReg(inst.rd, (rs1_val < static_cast<uint32_t>(inst.imm)) ? 1u : 0u);
                cpu.setPC(pc + 4);
                break;
            case INST_LUI:
                cpu.setReg(inst.rd, static_cast<uint32_t>(inst.imm));
                cpu.setPC(pc + 4);
                break;
            case INST_AUIPC:
                cpu.setReg(inst.rd, pc + static_cast<uint32_t>(inst.imm));
                cpu.setPC(pc + 4);
                break;
            case INST_BEQ:
                cpu.setPC((rs1_val == rs2_val) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_BNE:
                cpu.setPC((rs1_val != rs2_val) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_BLT:
                cpu.setPC((static_cast<int32_t>(rs1_val) < static_cast<int32_t>(rs2_val)) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_BGE:
                cpu.setPC((static_cast<int32_t>(rs1_val) >= static_cast<int32_t>(rs2_val)) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_BLTU:
                cpu.setPC((rs1_val < rs2_val) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_BGEU:
                cpu.setPC((rs1_val >= rs2_val) ? (pc + inst.imm) : (pc + 4));
                break;
            case INST_JAL:
                cpu.setReg(inst.rd, pc + 4);
                cpu.setPC(pc + inst.imm);
                break;
            case INST_JALR:
                cpu.setReg(inst.rd, pc + 4);
                cpu.setPC((rs1_val + inst.imm) & ~1u);
                break;
            case INST_MUL: {
                int32_t a = static_cast<int32_t>(rs1_val);
                int32_t b = static_cast<int32_t>(rs2_val);
                cpu.setReg(inst.rd, static_cast<uint32_t>(a * b));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_MULH: {
                int64_t a = static_cast<int64_t>(static_cast<int32_t>(rs1_val));
                int64_t b = static_cast<int64_t>(static_cast<int32_t>(rs2_val));
                int64_t r = a * b;
                cpu.setReg(inst.rd, static_cast<uint32_t>(r >> 32));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_MULHSU: {
                int64_t a = static_cast<int64_t>(static_cast<int32_t>(rs1_val));
                uint64_t b = static_cast<uint64_t>(rs2_val);
                int64_t r = a * static_cast<int64_t>(b);
                cpu.setReg(inst.rd, static_cast<uint32_t>(r >> 32));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_MULHU: {
                uint64_t a = static_cast<uint64_t>(rs1_val);
                uint64_t b = static_cast<uint64_t>(rs2_val);
                uint64_t r = a * b;
                cpu.setReg(inst.rd, static_cast<uint32_t>(r >> 32));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_DIV: {
                int32_t a = static_cast<int32_t>(rs1_val);
                int32_t b = static_cast<int32_t>(rs2_val);
                if (b == 0) cpu.setReg(inst.rd, 0xFFFFFFFFu);
                else if (a == static_cast<int32_t>(0x80000000u) && b == -1)
                    cpu.setReg(inst.rd, 0x80000000u);
                else
                    cpu.setReg(inst.rd, static_cast<uint32_t>(a / b));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_DIVU: {
                if (rs2_val == 0) cpu.setReg(inst.rd, 0xFFFFFFFFu);
                else cpu.setReg(inst.rd, rs1_val / rs2_val);
                cpu.setPC(pc + 4);
                break;
            }
            case INST_REM: {
                int32_t a = static_cast<int32_t>(rs1_val);
                int32_t b = static_cast<int32_t>(rs2_val);
                if (b == 0) cpu.setReg(inst.rd, rs1_val);
                else if (a == static_cast<int32_t>(0x80000000u) && b == -1)
                    cpu.setReg(inst.rd, 0u);
                else
                    cpu.setReg(inst.rd, static_cast<uint32_t>(a % b));
                cpu.setPC(pc + 4);
                break;
            }
            case INST_REMU: {
                if (rs2_val == 0) cpu.setReg(inst.rd, rs1_val);
                else cpu.setReg(inst.rd, rs1_val % rs2_val);
                cpu.setPC(pc + 4);
                break;
            }
            case INST_ECALL:
            case INST_EBREAK:
            default:
                return false;
        }
        return true;
    }
};

class Emulator {
private:
    CPU cpu;
    Memory memory;
    std::unique_ptr<CacheLRU> cache_lru;
    std::unique_ptr<CacheBitPseudoLRU> cache_bplru;
    Decoder decoder;
    uint32_t return_address;

    uint32_t fetchInstruction(uint32_t pc) {
        uint32_t instr_lru = cache_lru->read(pc, 4, true);
        cache_bplru->read(pc, 4, true);
        return instr_lru;
    }

public:
    Emulator() : cache_lru(nullptr), cache_bplru(nullptr), return_address(0) {
        cache_lru = std::make_unique<CacheLRU>(&memory);
        cache_bplru = std::make_unique<CacheBitPseudoLRU>(&memory);
    }
    void loadInput(const InputData& input) {
        cpu.loadState(input.pc, input.registers);
        memory.loadFragments(input.fragments);
        return_address = input.registers[0];
    }
    void run() {
        while (true) {
            uint32_t pc = cpu.getPC();
            if (pc == return_address) break;
            uint32_t instruction = fetchInstruction(pc);
            DecodedInstruction decoded = decoder.decode(instruction);
            if (decoded.type == INST_ECALL || decoded.type == INST_EBREAK) break;

            if (decoded.type == INST_LB || decoded.type == INST_LH || decoded.type == INST_LW ||
                decoded.type == INST_LBU || decoded.type == INST_LHU) {
                uint32_t rs1_val = cpu.getReg(decoded.rs1);
                uint32_t addr = rs1_val + decoded.imm;
                uint32_t size =
                    (decoded.type == INST_LB || decoded.type == INST_LBU) ? 1u :
                    (decoded.type == INST_LH || decoded.type == INST_LHU) ? 2u : 4u;
                uint32_t value_lru = cache_lru->read(addr, size, false);
                cache_bplru->read(addr, size, false);
                if (decoded.type == INST_LB)
                    cpu.setReg(decoded.rd, static_cast<uint32_t>(static_cast<int8_t>(value_lru)));
                else if (decoded.type == INST_LH)
                    cpu.setReg(decoded.rd, static_cast<uint32_t>(static_cast<int16_t>(value_lru)));
                else if (decoded.type == INST_LBU)
                    cpu.setReg(decoded.rd, static_cast<uint8_t>(value_lru));
                else if (decoded.type == INST_LHU)
                    cpu.setReg(decoded.rd, static_cast<uint16_t>(value_lru));
                else
                    cpu.setReg(decoded.rd, value_lru);
                cpu.setPC(pc + 4);
            } else if (decoded.type == INST_SB || decoded.type == INST_SH || decoded.type == INST_SW) {
                uint32_t rs1_val = cpu.getReg(decoded.rs1);
                uint32_t rs2_val = cpu.getReg(decoded.rs2);
                uint32_t addr = rs1_val + decoded.imm;
                uint32_t size = (decoded.type == INST_SB) ? 1u : (decoded.type == INST_SH) ? 2u : 4u;
                cache_lru->write(addr, rs2_val, size);
                cache_bplru->write(addr, rs2_val, size);
                cpu.setPC(pc + 4);
            } else {
                if (!Executor::execute(decoded, cpu, memory)) break;
            }
        }
    }
    void saveOutput(const std::string& filename, uint32_t address, uint32_t size) {
        cache_lru->flush();
        cache_bplru->flush();
        uint32_t out_pc;
        uint32_t out_regs[31];
        cpu.saveState(out_pc, out_regs);
        writeOutputFile(filename, out_pc, out_regs, address, size, memory.getData());
    }
    const CacheStats& getLRUStats() const { return cache_lru->getStats(); }
    const CacheStats& getBitPseudoLRUStats() const { return cache_bplru->getStats(); }
};

struct Arguments {
    std::string input_file;
    bool has_output;
    std::string output_file;
    uint32_t output_address;
    uint32_t output_size;
};

static Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;
    args.has_output = false;
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "-i") {
            if (i + 1 >= argc) throw std::runtime_error("Missing argument for -i");
            args.input_file = argv[i + 1];
            i += 2;
        } else if (arg == "-o") {
            if (i + 3 >= argc) throw std::runtime_error("Missing arguments for -o");
            args.has_output = true;
            args.output_file = argv[i + 1];
            args.output_address = parseNumber(argv[i + 2]);
            args.output_size = parseNumber(argv[i + 3]);
            i += 4;
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (args.input_file.empty()) throw std::runtime_error("Input file (-i) is required");
    return args;
}

static void printStats(const CacheStats& lru_stats, const CacheStats& bplru_stats) {
    printf("| replacement | hit_rate | instr_hit_rate | data_hit_rate | instr_access |  instr_hit   | data_access  |   data_hit   |\n");
    printf("| :---------- | :------: | -------------: | ------------: | -----------: | -----------: | -----------: | -----------: |\n");

    uint64_t lru_total_access = lru_stats.instr_access + lru_stats.data_access;
    uint64_t lru_total_hit = lru_stats.instr_hit + lru_stats.data_hit;
    double lru_hit_rate = (lru_total_access > 0) ? (100.0 * lru_total_hit / lru_total_access) : NAN;
    double lru_instr_hit_rate = (lru_stats.instr_access > 0) ? (100.0 * lru_stats.instr_hit / lru_stats.instr_access) : NAN;
    double lru_data_hit_rate = (lru_stats.data_access > 0) ? (100.0 * lru_stats.data_hit / lru_stats.data_access) : NAN;

    if (lru_total_access == 0) {
        printf("| LRU         |    nan%% |          nan%% |         nan%% | %12d | %12d | %12d | %12d |\n", 0, 0, 0, 0);
    } else {
        printf("| LRU         | %3.4f%% |       %3.4f%% |      %3.4f%% | %12llu | %12llu | %12llu | %12llu |\n",
               lru_hit_rate, lru_instr_hit_rate, lru_data_hit_rate,
               (unsigned long long)lru_stats.instr_access, (unsigned long long)lru_stats.instr_hit,
               (unsigned long long)lru_stats.data_access, (unsigned long long)lru_stats.data_hit);
    }

    uint64_t bplru_total_access = bplru_stats.instr_access + bplru_stats.data_access;
    uint64_t bplru_total_hit = bplru_stats.instr_hit + bplru_stats.data_hit;
    double bplru_hit_rate = (bplru_total_access > 0) ? (100.0 * bplru_total_hit / bplru_total_access) : NAN;
    double bplru_instr_hit_rate = (bplru_stats.instr_access > 0) ? (100.0 * bplru_stats.instr_hit / bplru_stats.instr_access) : NAN;
    double bplru_data_hit_rate = (bplru_stats.data_access > 0) ? (100.0 * bplru_stats.data_hit / bplru_stats.data_access) : NAN;

    if (bplru_total_access == 0) {
        printf("| bpLRU       |    nan%% |          nan%% |         nan%% | %12d | %12d | %12d | %12d |\n", 0, 0, 0, 0);
    } else {
        printf("| bpLRU       | %3.4f%% |       %3.4f%% |      %3.4f%% | %12llu | %12llu | %12llu | %12llu |\n",
               bplru_hit_rate, bplru_instr_hit_rate, bplru_data_hit_rate,
               (unsigned long long)bplru_stats.instr_access, (unsigned long long)bplru_stats.instr_hit,
               (unsigned long long)bplru_stats.data_access, (unsigned long long)bplru_stats.data_hit);
    }
}

int main(int argc, char* argv[]) {
    try {
        Arguments args = parseArguments(argc, argv);
        InputData input = readInputFile(args.input_file);
        Emulator emulator;
        emulator.loadInput(input);
        emulator.run();
        if (args.has_output) emulator.saveOutput(args.output_file, args.output_address, args.output_size);
        printStats(emulator.getLRUStats(), emulator.getBitPseudoLRUStats());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}