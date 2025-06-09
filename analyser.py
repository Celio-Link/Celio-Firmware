import sys

# Ensure a filename is provided
if len(sys.argv) < 2:
    print("Usage: python read_chunks.py <filename>")
    sys.exit(1)

filename = sys.argv[1]

try:
    # Read file into an array (list of lines)
    with open(filename, 'r', encoding='utf-8') as file:
        lines = [line.rstrip('\n') for line in file]

except FileNotFoundError:
    print(f"File '{filename}' not found.")


lines.pop(0)
lines = [s.removeprefix("\"Async Serial\",") for s in lines]

with open(filename + ".bak", "w") as output:

    chunk_size = 18
    for i in range(0, len(lines), chunk_size):
        chunk = lines[i:i + chunk_size]
        outputChunk = []
        if all(line.strip() == "0x0000" for line in chunk):
            outputChunk = ["\n"] * 9
        else:
            for i in range(0, len(chunk), 2):
                chunk[i] += ("   " + chunk[i + 1])
                outputChunk.append(chunk[i] + "\n")
        
        output.writelines(outputChunk)

print("Done")