IN_NAME = "parsed.txt"
OUT_NAME = "SOKOLVLS.bin"
WIDTH = 19
HEIGHT = 16

with open(IN_NAME, "r") as f_in:
    level_rows_list = []

    for level in f_in.read().split("\n\n")[:20]:
        rows = level.split("\n")
        i = 0
        for i, row in enumerate(rows):
            level_rows_list.append(row.ljust(WIDTH))
        for i in range(HEIGHT-i-1):
            level_rows_list.append(WIDTH * " ")
        


    with open(OUT_NAME, "w") as f_out:
        for row in level_rows_list:
            f_out.write(row)

