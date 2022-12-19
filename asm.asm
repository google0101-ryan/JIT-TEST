bits 64

start:
	jnz label
	lea rax, [rbp + 0x8]
	add word[rax], 0xFFFF
label:
	hlt