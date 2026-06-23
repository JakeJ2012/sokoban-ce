global_max_w = 0
global_max_h = 0

current_w = 0
current_h = 0
levels_checked = 0

with open("parsed.txt", "r") as f:
    for line in f:
        line = line.rstrip("\n")

        # Blank line = end of a level
        if line == "":
            # Update global max from this level
            if current_w > global_max_w:
                global_max_w = current_w
            if current_h > global_max_h:
                global_max_h = current_h

            levels_checked += 1
            if levels_checked >= 20:
                break

            # Reset for next level
            current_w = 0
            current_h = 0
            continue

        # Update width
        if len(line) > current_w:
            current_w = len(line)

        # Update height (line count)
        current_h += 1

# Handle last level if file doesn't end with blank line
if current_w > global_max_w:
    global_max_w = current_w
if current_h > global_max_h:
    global_max_h = current_h

print("Largest width:", global_max_w)
print("Largest height:", global_max_h)
