#include <iostream>
#include <fstream>
#include <string>

int e462counter = 0;
using namespace std;

uint8_t RAM[2048];
uint8_t ROM[32768];
uint8_t PPU[8];
uint8_t SaveWorkRAM[8191];

// Registers
uint8_t A; // Accumulator
uint8_t X; // Index
uint8_t Y; // Index
uint16_t PC; // Program Counter
uint8_t SP; // Stack Pointer
//uint8_t P; // Status // 5th bit should be 1

// Status Register
uint8_t C, Z, I, D, B, V, N; // Carry Flag, Zero Flag, Interrupt Disable, Decimal Mode Flag, Break Command, Overflow Flag, Negative Flag

const char* szInstruction;
uint16_t logAddress;
uint8_t operands[3];

enum AddressingMode
{
	IMPLICIT,
	ACCUMULATOR,
	IMMEDIATE,
	ZEROPAGE,
	ZEROPAGEX,
	ZEROPAGEY,
	RELATIVE,
	ABSOLUTE,
	ABSOLUTEX,
	ABSOLUTEY,
	INDIRECT,
	INDIRECTX,
	INDIRECTY
};

AddressingMode addressingMode;

void SetInstruction(char* szName)
{
	szInstruction = szName;
}

ofstream logfile;

char logStatusBuffer[32];
char logAXY[32];

void LogInstruction()
{
	static char temp1[32];
	static char logBuffer[128];

	switch (addressingMode)
	{
		case IMPLICIT:
			sprintf(temp1, "");
			break;
		case ACCUMULATOR:
			sprintf(temp1, "A");
			break;
		case IMMEDIATE:
			sprintf(temp1, "#$%02x", operands[0]);
			break;
		case ZEROPAGE:
			sprintf(temp1, "$%02x", operands[0]);
			break;
		case ZEROPAGEX:
			sprintf(temp1, "$%02x,X", operands[0]);
			break;
		case ZEROPAGEY:
			sprintf(temp1, "$%02x,Y", operands[0]);
			break;
		case RELATIVE:
			sprintf(temp1, "*%d", static_cast<int8_t>(operands[0]));
			break;
		case ABSOLUTE:
			sprintf(temp1, "$%02x $%02x", operands[0], operands[1]);
			break;
		case ABSOLUTEX:
			sprintf(temp1, "$%02x $%02x,X", operands[0], operands[1]);
			break;
		case ABSOLUTEY:
			sprintf(temp1, "$%02x $%02x,Y", operands[0], operands[1]);
			break;
		case INDIRECT:
			sprintf(temp1, "($%02x $%02x)", operands[0], operands[1]);
			break;
		case INDIRECTX:
			sprintf(temp1, "($%02x,X)", operands[0]);
			break;
		case INDIRECTY:
			sprintf(temp1, "($%02x),Y", operands[0]);
			break;
		default:
			break;
	}
	
	//sprintf(logStatusBuffer, "%c%c%c%c%c%c%c%c", N ? 'N' : 'n', V ? 'V' : 'v', 'U', 'B', D ? 'D' : 'd', I ? 'I' : 'i', Z ? 'Z' : 'z', C ? 'C' : 'c');

	uint8_t P = C | (Z << 1) | (I << 2) | (D << 3) | (V << 6) | (N << 7);
	sprintf(logBuffer, "%04x %s %s %s P: %s\n", logAddress, szInstruction, temp1, logAXY, logStatusBuffer);

	if (logfile)
		logfile << logBuffer;
	else
		printf(logBuffer);
}

uint8_t ReadMemory(uint16_t address)
{
    // 2k internal ram (valid range 0x0 to 0x07FF)
    // Values over 0x07FF wrap back to 0 (are mirrored)
    if (address >= 0 && address < 0x2000)
    {
        return RAM[address & 0x7FF];
    }
	else if (address >= 0x2000 && address < 0x4000)
	{
		// Mirror 0x2000 to 0x2007
		uint8_t value = PPU[address & 0x0007];

		// Rest last bit
		if ((0x2000 + (address & 0x0007)) == 0x2002)
		{
			PPU[address & 0x0007] = value & 0x7F;
		}

		return value;
	}
	else if (address >= 0x6000 && address <= 0x7FFF)
	{
		return SaveWorkRAM[address - 0x6000];
	}
	else if (address >= 0x8000 && address <= 0xFFFF)
	{
		if (address >= 0xC000)
		{
			return ROM[address - 0xC000];
		}
		else
		{
			return ROM[address - 0x8000];
		}
	}
    
    return 0;
}

void WriteMemory(uint16_t address, uint8_t value)
{
    if (address >= 0 && address < 0x2000)
    {
        RAM[address & 0x07FF] = value;
    }
	else if (address >= 0x2000 && address < 0x4000)
	{
		// Mirror 0x2000 to 0x2007
		PPU[address & 0x0007] = value;
	}
	else if (address >= 0x6000 && address <= 0x7FFF)
	{
		SaveWorkRAM[address - 0x6000] = value;
	}
	else if (address >= 0x8000 && address <= 0xFFFF)
	{
		ROM[address - 0x8000] = value;
	}
	else
	{
		int a = 0;
	}
}

uint8_t PullStack()
{
	++SP;
	return ReadMemory(0x100 + SP);
}

void PushStack(uint8_t value)
{
	WriteMemory(0x100 + SP, value);
	--SP;
}

// Addressing Modes

void Implicit()
{
	addressingMode = IMPLICIT;
}

uint8_t Accumulator()
{
	addressingMode = ACCUMULATOR;

    return A;
}

uint8_t Immediate()
{
	addressingMode = IMMEDIATE;

    uint8_t value = ReadMemory(PC++);

	operands[0] = value;

	return value;
}

uint16_t ZeroPageAddress()
{
	addressingMode = ZEROPAGE;

    uint8_t address = ReadMemory(PC++);

	operands[0] = address;

	return address;
}

uint8_t ZeroPage()
{
    return ReadMemory(ZeroPageAddress());
}

uint16_t ZeroPageXAddress()
{
	addressingMode = ZEROPAGEX;

    uint8_t address = ReadMemory(PC++);

	operands[0] = address;

    return ((address + X) % 256);
}

uint8_t ZeroPageX()
{
    return ReadMemory(ZeroPageXAddress());
}

uint16_t ZeroPageYAddress()
{
	addressingMode = ZEROPAGEY;

	uint8_t address = ReadMemory(PC++);

	operands[0] = address;

	return ((address + Y) % 256);
}

uint8_t ZeroPageY()
{
	return ReadMemory(ZeroPageYAddress());
}

uint8_t Relative()
{
	addressingMode = RELATIVE;

	uint8_t offset = ReadMemory(PC++);

	operands[0] = offset;

	return offset;
}

uint16_t AbsoluteAddress()
{
	addressingMode = ABSOLUTE;

    uint8_t low = ReadMemory(PC++);
    uint8_t high = ReadMemory(PC++);

	operands[0] = low;
	operands[1] = high;

    return (low | (high << 8));
}

uint8_t Absolute()
{
    return ReadMemory(AbsoluteAddress());
}

uint16_t AbsoluteXAddress()
{
	addressingMode = ABSOLUTEX;

    uint8_t low = ReadMemory(PC++);
    uint8_t high = ReadMemory(PC++);
    uint16_t address = low | (high << 8);

	operands[0] = low;
	operands[1] = high;

    return address + X;
}

uint8_t AbsoluteX()
{
    return ReadMemory(AbsoluteXAddress());
}

uint16_t AbsoluteYAddress()
{
	addressingMode = ABSOLUTEY;

    uint8_t low = ReadMemory(PC++);
    uint8_t high = ReadMemory(PC++);
    uint16_t address = low | (high << 8);

	operands[0] = low;
	operands[1] = high;

    return address + Y;
}

uint8_t AbsoluteY()
{
	return ReadMemory(AbsoluteYAddress());
}

uint16_t IndirectAddress()
{
	addressingMode = INDIRECT;

    uint8_t low = ReadMemory(PC++);
    uint8_t high = ReadMemory(PC++);

	operands[0] = low;
	operands[1] = high;

	uint16_t address = (low | (high << 8));
    low = ReadMemory(address);

	// If the address is on a page boundary 0x??FF then it changes to 0x??00 for the high byte
	// Example 0x02FF becomes 0x0200
	if ((address & 0xFF) == 0xFF)
	{
		high = ReadMemory(address & 0xFF00);
	}
	else
	{
		high = ReadMemory(address + 1);
	}

    return (low | (high << 8));
}

uint16_t IndirectXAddress()
{
	addressingMode = INDIRECTX;

	uint8_t temp = ReadMemory(PC++);
	uint8_t low = ReadMemory((temp + X) % 256);
	uint8_t high = ReadMemory((temp + 1 + X) % 256);

	operands[0] = temp;

	return low | (high << 8);
}

uint8_t IndirectX()
{
    return ReadMemory(IndirectXAddress());
}

uint16_t IndirectYAddress()
{
	addressingMode = INDIRECTY;

	uint8_t temp = ReadMemory(PC++);
	uint8_t low = ReadMemory(temp);
	uint8_t high = ReadMemory((temp + 1) % 256);

	operands[0] = temp;

	return (low | (high << 8)) + Y;
}

uint8_t IndirectY()
{  
    return ReadMemory(IndirectYAddress());
}

// ADC (Add with carry)
void ADC(uint8_t value)
{
    uint16_t result = A + value + C;
    
    // Carry flag
    C = (result & 0x100) >> 8;
    
    // Overflow flag
    V = (((A ^ result) & (value ^ result) & 0x80) >> 7) & 0x01;
    
    A = result & 0xFF;

	// Zero flag
	Z = A == 0 ? 1 : 0;

	// Negative flag
	N = (A >> 7) & 0x1;

	SetInstruction("ADC");
}

// AND (Logical AND)
void AND(uint8_t value)
{
    A &= value;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("AND");
}

// Arithmetic Shift Left
void ASL_A()
{
	// Carry flag
	C = ((A & 0x80) >> 7) & 0x01;

	A = A << 1;

	// Zero flag
	Z = A == 0 ? 1 : 0;

	// Negative flag
	N = (A >> 7) & 0x1;

	SetInstruction("ASL");
}

void ASL(uint16_t address)
{
	uint8_t value = ReadMemory(address);

	// Carry flag
	C = ((value & 0x80) >> 7) & 0x01;

    value = value << 1;
    
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;

	WriteMemory(address, value);
    
	SetInstruction("ASL");
}

// Branch if Carry Clear
void BCC(uint8_t value)
{
    if (!C)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BCC");
}

// Branch if Carry Set
void BCS(uint8_t value)
{
    if (C)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BCS");
}

// Branch if Equal
void BEQ(uint8_t value)
{
    if(Z)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BEQ");
}

// Bit Test
void BIT(uint8_t value)
{
    uint8_t result = A & value;
    
    // Zero flag
    Z = result == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;
    
    // Overflow flag
    V = (value >> 6) & 0x1;

	SetInstruction("BIT");
}

void BMI(uint8_t value)
{
    if (N)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BMI");
}

void BNE(uint8_t value)
{
    if(!Z)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BNE");
}

void BPL(uint8_t value)
{
    if (!N)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BPL");
}

void BRK()
{
	uint8_t low = PC & 0xFF;
	uint8_t high = (PC >> 8) & 0xFF;

	PushStack(low);
	PushStack(high);

	uint8_t P = C | (Z << 1) | (I << 2) | (D << 3) | (V << 6) | (N << 7);
	P |= (1 << 4);
	P |= (1 << 5);

	PushStack(P);

	SetInstruction("BRK");
}

void BVC(uint8_t value)
{
    if(!V)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BVC");
}

void BVS(uint8_t value)
{
    if (V)
    {
        PC += static_cast<int8_t>(value);
    }

	SetInstruction("BVS");
}

void CLC()
{
    C = 0;

	SetInstruction("CLC");
}

void CLD()
{
    D = 0;

	SetInstruction("CLD");
}

void CLI()
{
    I = 0;

	SetInstruction("CLI");
}

void CLV()
{
    V = 0;

	SetInstruction("CLV");
}

void CMP(uint8_t value)
{
    // Carry flag
    C = A >= value;
    
    // Zero flag
    Z = A == value;
    
    uint8_t result = A - value;
    
    // Negative flag
    N = (result >> 7) & 0x1;

	SetInstruction("CMP");
}

void CPX(uint8_t value)
{
    // Carry flag
    C = X >= value;
    
    // Zero flag
    Z = X == value;
    
    uint8_t result = X - value;
    
    // Negative flag
    N = (result >> 7) & 0x1;

	SetInstruction("CPX");
}

void CPY(uint8_t value)
{
    // Carry flag
    C = Y >= value;
    
    // Zero flag
    Z = Y == value;
    
    uint8_t result = Y - value;
    
    // Negative flag
    N = (result >> 7) & 0x1;

	SetInstruction("CPY");
}

void DEC(uint16_t address)
{
    uint8_t value = ReadMemory(address);
    value -= 1;
    
    WriteMemory(address, value);
    
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;

	SetInstruction("DEC");
}

void DEX()
{
    X -= 1;
    
    // Zero flag
    Z = X == 0 ? 1 : 0;
    
    // Negative flag
    N = (X >> 7) & 0x1;

	SetInstruction("DEX");
}

void DEY()
{
    Y -= 1;
    
    // Zero flag
    Z = Y == 0 ? 1 : 0;
    
    // Negative flag
    N = (Y >> 7) & 0x1;

	SetInstruction("DEY");
}

void EOR(uint8_t value)
{
    A = A ^ value;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("EOR");
}

void INC(uint16_t address)
{
    uint8_t value = ReadMemory(address);
    value += 1;
    WriteMemory(address, value);
    
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;

	SetInstruction("INC");
}

void INX()
{
    X += 1;
    
    // Zero flag
    Z = X == 0 ? 1 : 0;
    
    // Negative flag
    N = (X >> 7) & 0x1;

	SetInstruction("INX");
}

void INY()
{
    Y += 1;
    
    // Zero flag
    Z = Y == 0 ? 1 : 0;
    
    // Negative flag
    N = (Y >> 7) & 0x1;

	SetInstruction("INY");
}

void JMP(uint16_t address)
{
    PC = address;

	SetInstruction("JMP");
}

// JSR (Jump To Subroutine
void JSR(uint16_t address)
{
    uint16_t temp = PC - 1;
    uint8_t low = temp & 0xFF;
    uint8_t high = (temp >> 8) & 0xFF;
    
	PushStack(high);
	PushStack(low);
	
    PC = address;

	SetInstruction("JSR");
}

void LDA(uint8_t value)
{
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;
    
    A = value;

	SetInstruction("LDA");
}

void LDX(uint8_t value)
{
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;
    
    X = value;

	SetInstruction("LDX");
}

void LDY(uint8_t value)
{
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = (value >> 7) & 0x1;
    
    Y = value;

	SetInstruction("LDY");
}

void LSR_A()
{
    // Carry flag
    C = A & 0x01;
    
    // Bit 7 is set to 0 after the shift
    A = (A >> 1) & 0x7F;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = 0;

	SetInstruction("LSR");
}

void LSR(uint16_t address)
{
    uint8_t value = ReadMemory(address);
    
    // Carry flag
    C = value & 0x01;
    
    // Bit 7 is set to 0 after the shift
    value = (value >> 1) & 0x7F;
    
    // Zero flag
    Z = value == 0 ? 1 : 0;
    
    // Negative flag
    N = 0;

	WriteMemory(address, value);

	SetInstruction("LSR");
}

void NOP()
{
	SetInstruction("NOP");
}

void ORA(uint8_t value)
{
    A = A | value;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("ORA");
}

void PHA()
{
	PushStack(A);

	SetInstruction("PHA");
}

void PHP()
{
    uint8_t P = C | ( Z << 1) | ( I << 2) | (D << 3) | (V << 6) | (N << 7);
    P |= (1 << 4);
    P |= (1 << 5);
    
	PushStack(P);

	SetInstruction("PHP");
}

void PLA()
{
	A = PullStack();
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("PLA");
}

void PLP()
{
	uint8_t P = PullStack();

	C = P & 0x01;
	Z = (P >> 1) & 0x01;
	I = (P >> 2) & 0x01;
	D = (P >> 3) & 0x01;
	V = (P >> 6) & 0x01;
	N = (P >> 7) & 0x01;

	SetInstruction("PLP");
}

void ROL_A()
{
	uint8_t newBitZero = C;

	// Carry flag becomes the old bit 7
	C = (A >> 7) & 0x01;

	A = (A << 1);

	// The first bit should be 0 now due to the bit shift.
	A |= newBitZero;

	// Zero flag
	Z = A == 0 ? 1 : 0;

	// Negative flag
	N = (A >> 7) & 0x1;

	SetInstruction("ROL");
}

void ROL(uint16_t address)
{
	uint8_t value = ReadMemory(address);

	uint8_t newBitZero = C;

	// Carry flag becomes the old bit 7
	C = (value >> 7) & 0x01;

	value = value << 1;

	// The first bit should be 0 now due to the bit shift.
	value |= newBitZero;

	// Zero flag
	Z = value == 0 ? 1 : 0;

	// Negative flag
	N = (value >> 7) & 0x1;

	WriteMemory(address, value);

	SetInstruction("ROL");
}

void ROR_A()
{
	uint8_t newLastBit = C;

	// Carry flag becomes the old bit 7
	C = A & 0x01;

	A = (A >> 1) & 0x7F;

	// The last bit should be 0 due to masking with 0x7F.
	A |= (newLastBit << 7);

	// Zero flag
	Z = A == 0 ? 1 : 0;

	// Negative flag
	N = (A >> 7) & 0x1;

	SetInstruction("ROR");
}

void ROR(uint16_t address)
{
	uint8_t value = ReadMemory(address);

	uint8_t newLastBit = C;

	// Carry flag becomes the old bit 7
	C = value & 0x01;

	value = (value >> 1) & 0x7F;

	// The last bit should be 0 due to masking with 0x7F.
	value |= (newLastBit << 7);

	// Zero flag
	Z = value == 0 ? 1 : 0;

	// Negative flag
	N = (value >> 7) & 0x1;

	WriteMemory(address, value);

	SetInstruction("ROR");
}

void RTI()
{
	uint8_t P = PullStack();
	C = P & 0x01;
	Z = (P >> 1) & 0x01;
	I = (P >> 2) & 0x01;
	D = (P >> 3) & 0x01;
	V = (P >> 6) & 0x01;
	N = (P >> 7) & 0x01;

	uint8_t low = PullStack();
	uint8_t high = PullStack();
	
	PC = low | (high << 8);

	SetInstruction("RTI");
}

void RTS()
{
	uint8_t low = PullStack();
	uint8_t high = PullStack();
	
	uint16_t address = low | (high << 8);
	PC = address + 1;

	SetInstruction("RTS");
}

void SBC(uint8_t value)
{
	value = value ^ 0xFF;

	uint16_t result = A + value + C;

	// Carry flag
	C = (result & 0x100) >> 8;

	// Overflow flag
	V = (((A ^ result) & (value ^ result) & 0x80) >> 7) & 0x01;

	A = result & 0xFF;

	// Zero flag
	Z = A == 0 ? 1 : 0;

	// Negative flag
	N = (A >> 7) & 0x1;

	SetInstruction("SBC");
}

void SEC()
{
	C = 1;

	SetInstruction("SEC");
}

void SED()
{
	D = 1;

	SetInstruction("SED");
}

void SEI()
{
	I = 1;

	SetInstruction("SEI");
}

void STA(uint16_t address)
{
	WriteMemory(address, A);

	SetInstruction("STA");
}

void STX(uint16_t address)
{
	WriteMemory(address, X);

	SetInstruction("STX");
}

void STY(uint16_t address)
{
	WriteMemory(address, Y);

	SetInstruction("STY");
}

void TAX()
{
    X = A;
    
    // Zero flag
    Z = X == 0 ? 1 : 0;
    
    // Negative flag
    N = (X >> 7) & 0x1;

	SetInstruction("TAX");
}

void TAY()
{
    Y = A;
    
    // Zero flag
    Z = Y == 0 ? 1 : 0;
    
    // Negative flag
    N = (Y >> 7) & 0x1;

	SetInstruction("TAY");
}

void TSX()
{
    X = SP;
    
    // Zero flag
    Z = X == 0 ? 1 : 0;
    
    // Negative flag
    N = (X >> 7) & 0x1;

	SetInstruction("TSX");
}

// TXA (Transfer X to Accumulator)
void TXA()
{
    A = X;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("TXA");
}

// TXS (Transfer X to Stack Pointer)
void TXS()
{
    SP = X;

	SetInstruction("TXS");
}

// TYA (Transfer Y to Accumulator)
void TYA()
{
    A = Y;
    
    // Zero flag
    Z = A == 0 ? 1 : 0;
    
    // Negative flag
    N = (A >> 7) & 0x1;

	SetInstruction("TYA");
}

void SimulatePPU()
{
	static int i = 0;
	++i;

	if (i == 1000)
	{
		WriteMemory(0x2002, 0x80);
		i = 0;
	}
}

void ProcessInstruction()
{
	logAddress = PC;

	if (PC == 0xE462)
		++e462counter;

	uint8_t opcode = ReadMemory(PC++);

	switch (opcode)
	{

		// ADC (Add with carry)
	case 0x69:
		ADC(Immediate());
		break;

	case 0x65:
		ADC(ZeroPage());
		break;

	case 0x75:
		ADC(ZeroPageX());
		break;

	case 0x6D:
		ADC(Absolute());
		break;

	case 0x7D:
		ADC(AbsoluteX());
		break;

	case 0x79:
		ADC(AbsoluteY());
		break;

	case 0x61:
		ADC(IndirectX());
		break;

	case 0x71:
		ADC(IndirectY());
		break;

		// AND
	case 0x29:
		AND(Immediate());
		break;

	case 0x25:
		AND(ZeroPage());
		break;

	case 0x35:
		AND(ZeroPageX());
		break;

	case 0x2D:
		AND(Absolute());
		break;

	case 0x3D:
		AND(AbsoluteX());
		break;

	case 0x39:
		AND(AbsoluteY());
		break;

	case 0x21:
		AND(IndirectX());
		break;

	case 0x31:
		AND(IndirectY());
		break;

		// ASL (Arithmetic Shift Left)
	case 0x0A:
		Accumulator();
		ASL_A();
		break;

	case 0x06:
		ASL(ZeroPageAddress());
		break;

	case 0x16:
		ASL(ZeroPageXAddress());
		break;

	case 0x0E:
		ASL(AbsoluteAddress());
		break;

	case 0x1E:
		ASL(AbsoluteXAddress());
		break;

		// BCC (Branch if Carry Clear)
	case 0x90:
		BCC(Relative());
		break;

		// BCS (Branch if Carry Set)
	case 0xB0:
		BCS(Relative());
		break;

		// BEQ (Branch if Equal)
	case 0xF0:
		BEQ(Relative());
		break;

		// BIT (Bit Test)
	case 0x24:
		BIT(ZeroPage());
		break;

	case 0x2C:
		BIT(Absolute());
		break;

		// BMI (Branch if Minus)
	case 0x30:
		BMI(Relative());
		break;

		// BNE (Branch if Not Equal)
	case 0xD0:
		BNE(Relative());
		break;

		// BPL (Branch if Positive)
	case 0x10:
		BPL(Relative());
		break;

		// BRK (Force Interrupt)
	case 0x00:
		Implicit();
		BRK();
		break;

		// BVC (Branch if Overflow Clear)
	case 0x50:
		BVC(Relative());
		break;

		// BVS (Branch if Overflow Set)
	case 0x70:
		BVS(Relative());
		break;

		// CLC (Clear Carry Flag)
	case 0x18:
		Implicit();
		CLC();
		break;

		// CLD (Clear Decimal Mode)
	case 0xD8:
		Implicit();
		CLD();
		break;

		// CLI (Clear Interrupt Disable)
	case 0x58:
		Implicit();
		CLI();
		break;

		// CLV (Clear Overflow Flag)
	case 0xB8:
		Implicit();
		CLV();
		break;

		// CMP (Compare)
	case 0xC9:
		CMP(Immediate());
		break;

	case 0xC5:
		CMP(ZeroPage());
		break;

	case 0xD5:
		CMP(ZeroPageX());
		break;

	case 0xCD:
		CMP(Absolute());
		break;

	case 0xDD:
		CMP(AbsoluteX());
		break;

	case 0xD9:
		CMP(AbsoluteY());
		break;

	case 0xC1:
		CMP(IndirectX());
		break;

	case 0xD1:
		CMP(IndirectY());
		break;

		// CPX
	case 0xE0:
		CPX(Immediate());
		break;

	case 0xE4:
		CPX(ZeroPage());
		break;

	case 0xEC:
		CPX(Absolute());
		break;

		// CPY
	case 0xC0:
		CPY(Immediate());
		break;

	case 0xC4:
		CPY(ZeroPage());
		break;

	case 0xCC:
		CPY(Absolute());
		break;

		// DEC (Decrement Memory)
	case 0xC6:
		DEC(ZeroPageAddress());
		break;

	case 0xD6:
		DEC(ZeroPageXAddress());
		break;

	case 0xCE:
		DEC(AbsoluteAddress());
		break;

	case 0xDE:
		DEC(AbsoluteXAddress());
		break;

		// DEX
	case 0xCA:
		Implicit();
		DEX();
		break;

		// DEY
	case 0x88:
		Implicit();
		DEY();
		break;
		// EOR
	case 0x49:
		EOR(Immediate());
		break;

	case 0x45:
		EOR(ZeroPage());
		break;

	case 0x55:
		EOR(ZeroPageX());
		break;

	case 0x4D:
		EOR(Absolute());
		break;

	case 0x5D:
		EOR(AbsoluteX());
		break;

	case 0x59:
		EOR(AbsoluteY());
		break;

	case 0x41:
		EOR(IndirectX());
		break;

	case 0x51:
		EOR(IndirectY());
		break;

		// INC
	case 0xE6:
		INC(ZeroPageAddress());
		break;

	case 0xF6:
		INC(ZeroPageXAddress());
		break;

	case 0xEE:
		INC(AbsoluteAddress());
		break;

	case 0xFE:
		INC(AbsoluteXAddress());
		break;

		// INX
	case 0xE8:
		Implicit();
		INX();
		break;

		// INY
	case 0xC8:
		Implicit();
		INY();
		break;

		// JMP
	case 0x4C:
		JMP(AbsoluteAddress());
		break;

	case 0x6C:
		JMP(IndirectAddress());
		break;

		// JSR
	case 0x20:
		JSR(AbsoluteAddress());
		break;

		// LDA
	case 0xA9:
		LDA(Immediate());
		break;
	case 0xA5:
		LDA(ZeroPage());
		break;
	case 0xB5:
		LDA(ZeroPageX());
		break;
	case 0xAD:
		LDA(Absolute());
		break;
	case 0xBD:
		LDA(AbsoluteX());
		break;
	case 0xB9:
		LDA(AbsoluteY());
		break;
	case 0xA1:
		LDA(IndirectX());
		break;
	case 0xB1:
		LDA(IndirectY());
		break;

		// LDX
	case 0xA2:
		LDX(Immediate());
		break;
	case 0xA6:
		LDX(ZeroPage());
		break;
	case 0xB6:
		LDX(ZeroPageY());
		break;
	case 0xAE:
		LDX(Absolute());
		break;
	case 0xBE:
		LDX(AbsoluteY());
		break;

		// LDY
	case 0xA0:
		LDY(Immediate());
		break;
	case 0xA4:
		LDY(ZeroPage());
		break;
	case 0xB4:
		LDY(ZeroPageX());
		break;
	case 0xAC:
		LDY(Absolute());
		break;
	case 0xBC:
		LDY(AbsoluteX());
		break;

		// LSR
	case 0x4A:
		Implicit();
		LSR_A();
		break;
	case 0x46:
		LSR(ZeroPageAddress());
		break;
	case 0x56:
		LSR(ZeroPageXAddress());
		break;
	case 0x4E:
		LSR(AbsoluteAddress());
		break;
	case 0x5E:
		LSR(AbsoluteXAddress());
		break;

		// NOP
	case 0xEA:
		Implicit();
		NOP();
		break;

		// ORA
	case 0x09:
		ORA(Immediate());
		break;
	case 0x05:
		ORA(ZeroPage());
		break;
	case 0x15:
		ORA(ZeroPageX());
		break;
	case 0x0D:
		ORA(Absolute());
		break;
	case 0x1D:
		ORA(AbsoluteX());
		break;
	case 0x19:
		ORA(AbsoluteY());
		break;
	case 0x01:
		ORA(IndirectX());
		break;
	case 0x11:
		ORA(IndirectY());
		break;

		// PHA
	case 0x48:
		Implicit();
		PHA();
		break;

		// PHP
	case 0x08:
		Implicit();
		PHP();
		break;

		// PLA
	case 0x68:
		Implicit();
		PLA();
		break;

		// PLP
	case 0x28:
		Implicit();
		PLP();
		break;

		// ROL
	case 0x2A:
		Accumulator();
		ROL_A();
		break;

	case 0x26:
		ROL(ZeroPageAddress());
		break;

	case 0x36:
		ROL(ZeroPageXAddress());
		break;

	case 0x2E:
		ROL(AbsoluteAddress());
		break;

	case 0x3E:
		ROL(AbsoluteXAddress());
		break;

		// ROR
	case 0x6A:
		Accumulator();
		ROR_A();
		break;

	case 0x66:
		ROR(ZeroPageAddress());
		break;

	case 0x76:
		ROR(ZeroPageXAddress());
		break;

	case 0x6E:
		ROR(AbsoluteAddress());
		break;

	case 0x7E:
		ROR(AbsoluteXAddress());
		break;

		// RTI
	case 0x40:
		Implicit();
		RTI();
		break;

		// RTS
	case 0x60:
		Implicit();
		RTS();
		break;

		// SBC
	case 0xE9:
		SBC(Immediate());
		break;
	case 0xE5:
		SBC(ZeroPage());
		break;
	case 0xF5:
		SBC(ZeroPageX());
		break;
	case 0xED:
		SBC(Absolute());
		break;
	case 0xFD:
		SBC(AbsoluteX());
		break;
	case 0xF9:
		SBC(AbsoluteY());
		break;
	case 0xE1:
		SBC(IndirectX());
		break;
	case 0xF1:
		SBC(IndirectY());
		break;

		// SEC
	case 0x38:
		Implicit();
		SEC();
		break;

		// SED
	case 0xF8:
		Implicit();
		SED();
		break;

		// SEI
	case 0x78:
		Implicit();
		SEI();
		break;

		// STA
	case 0x85:
		STA(ZeroPageAddress());
		break;
	case 0x95:
		STA(ZeroPageXAddress());
		break;
	case 0x8D:
		STA(AbsoluteAddress());
		break;
	case 0x9D:
		STA(AbsoluteXAddress());
		break;
	case 0x99:
		STA(AbsoluteYAddress());
		break;
	case 0x81:
		STA(IndirectXAddress());
		break;
	case 0x91:
		STA(IndirectYAddress());
		break;

		// STX
	case 0x86:
		STX(ZeroPageAddress());
		break;
	case 0x96:
		STX(ZeroPageYAddress());
		break;
	case 0x8E:
		STX(AbsoluteAddress());
		break;

		// STY
	case 0x84:
		STY(ZeroPageAddress());
		break;
	case 0x94:
		STY(ZeroPageXAddress());
		break;
	case 0x8C:
		STY(AbsoluteAddress());
		break;

		// TAX
	case 0xAA:
		Implicit();
		TAX();
		break;

		// TAY
	case 0xA8:
		Implicit();
		TAY();
		break;

		// TSX
	case 0xBA:
		Implicit();
		TSX();
		break;

		// TXA
	case 0x8A:
		Implicit();
		TXA();
		break;

		// TXS
	case 0x9A:
		Implicit();
		TXS();
		break;

		// TYA (Transfer Y to Accumulator)
	case 0x98:
		Implicit();
		TYA();
		break;

	default:
		SetInstruction("UNKNOWN");
		break;
	}
}

void Initialize()
{
	PC = 0;
	SP = 0xFF;
	A = 0;
	C = 0;
	Z = 0;
	I = 0;
	D = 0;
	B = 0;
	V = 0;
	N = 0;

	memset(RAM, 0, sizeof(RAM));
	memset(ROM, 0, sizeof(ROM));
}

uint16_t GetResetVector()
{
	uint16_t offset = 0x8000;
	uint8_t low = ROM[0xfffc - offset];
	uint8_t high = ROM[0xfffd - offset];
	return low | (high << 8);
}

int main(int argc, const char * argv[])
{
	Initialize();
	
	ifstream file;
	file.open("official_only.nes", std::ios::binary);
	//file.open("01-basics.nes", std::ios::binary);
	//file.open("02-implied.nes", std::ios::binary);
	//file.open("03-immediate.nes", std::ios::binary);
	//file.open("04-zero_page.nes", std::ios::binary);
	//file.open("06-absolute.nes", std::ios::binary);
	//file.open("nestest.nes", std::ios::binary);

	if (file)
	{
		// Skip the header
		file.seekg(16, std::ios::beg);
		file.read((char*)ROM, sizeof(ROM));
	}

	file.close();

	PC = GetResetVector();

	logfile.open("log.txt", std::ios::trunc);

	for (int i = 0; i < 10000000; ++i)
	{
		//sprintf(logStatusBuffer, "%c%c%c%c%c%c%c%c", N ? 'N' : 'n', V ? 'V' : 'v', 'U', 'B', D ? 'D' : 'd', I ? 'I' : 'i', Z ? 'Z' : 'z', C ? 'C' : 'c');
		//sprintf(logAXY, "A %02x, X %02x, Y %02x, SP %02x", A, X, Y, SP);

		ProcessInstruction();

		//LogInstruction();
		SimulatePPU();
	}

	logfile.close();

	uint8_t low = ReadMemory(0x02);
	uint8_t high = ReadMemory(0x03);
	char error[32];
	sprintf(error, "%2x%2x", high, low);
    cout << "Hello World!" << endl;
	std::string results((char*)(SaveWorkRAM + 4));
	cout << results;
    cin.get();
    
    return 0;
}