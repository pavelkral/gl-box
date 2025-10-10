import os

glbox_dir = "glbox"

for root, dirs, files in os.walk(glbox_dir):
    for file in files:
        if file.endswith(".h"):
            header_path = os.path.join(root, file)

            cpp_path = os.path.splitext(header_path)[0] + ".cpp"
            content = f'#include "{file}"\n'

            if not os.path.exists(cpp_path):
                with open(cpp_path, "w") as f:
                    f.write(content)
                print(f"created: {cpp_path}")
            else:
                print(f"exist: {cpp_path}")
