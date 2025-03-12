import base64
import os

def generate_resource_header(input_dir, output_file):
    # Maximum length of a string literal (accounting for MSVC limits)
    MAX_STRING_LENGTH = 16000  # Reduced to account for escaping and compiler limits
    
    header_content = "#ifndef RESOURCES_H\n#define RESOURCES_H\n\n#include <string>\n#include <vector>\n\nnamespace Resources {\n"
    
    for root, _, files in os.walk(input_dir):
        for filename in files:
            filepath = os.path.join(root, filename)
            relative_path = os.path.relpath(filepath, input_dir).replace("\\", "/")
            with open(filepath, "rb") as f:
                encoded_data = base64.b64encode(f.read()).decode("utf-8")
            
            # Split the encoded data into chunks
            chunks = [encoded_data[i:i + MAX_STRING_LENGTH] for i in range(0, len(encoded_data), MAX_STRING_LENGTH)]
            
            # Create a vector of chunks
            var_name = filename.replace(".", "_").upper()
            header_content += f'    const std::vector<std::string> {var_name}_CHUNKS = {{\n'
            for i, chunk in enumerate(chunks):
                header_content += f'        "{chunk}"'
                if i < len(chunks) - 1:
                    header_content += ","
                header_content += "\n"
            header_content += "    };\n"
            
            # Add the path
            header_content += f'    const std::string {var_name}_PATH = "{relative_path}";\n'
    
    header_content += "}\n\n#endif // RESOURCES_H\n"
    
    with open(output_file, "w") as f:
        f.write(header_content)

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python generate_resources.py <input_dir> <output_file>")
        sys.exit(1)
    generate_resource_header(sys.argv[1], sys.argv[2])