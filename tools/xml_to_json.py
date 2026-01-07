import xmltodict
import json
import argparse
import os
import logging
import re

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

def parse_size(size_str):
    """Parses size attribute like '[1 N]' and returns N as an integer."""
    match = re.match(r"\[\s*\d+\s+(\d+)\s*\]", size_str)
    return int(match.group(1)) if match else None

def convert_double(value, size):
    """Converts a space-separated string of doubles into a float or a list of floats."""
    try:
        # Normalize the value: remove brackets & trim spaces
        cleaned_value = value.strip().replace("[", "").replace("]", "")
        
        numbers = list(map(float, cleaned_value.split()))
        return numbers[0] if size == 1 else numbers

    except ValueError:
        logging.warning(f"Failed to convert '{value}' to float(s). Keeping as string.")
        return value


def clean_structure(data, fields_to_skip):
    """
    Recursively processes the XML dictionary:
    - Removes attributes
    - Converts `double` values based on `size` attribute
    - Converts lists of `<cell>` elements into JSON arrays
    """
    if isinstance(data, dict):
        cleaned_data = {}
        size_info = None
        value_holder = None

        for key, value in data.items():
            if key.startswith("@"):
                if key == "@size":  # Extract size attribute
                    size_info = parse_size(value)
                continue  # Remove attributes

            if key in fields_to_skip:
                continue  # Skip unwanted fields

            cleaned_value = clean_structure(value, fields_to_skip)

            if key == "#text":
                value_holder = cleaned_value  # Store text content separately

            elif key == "cell":
                return cleaned_value if isinstance(cleaned_value, list) else [cleaned_value]

            else:
                cleaned_data[key] = cleaned_value

        # Handle `double` conversion if a size attribute is present
        if size_info is not None and value_holder is not None:
            return convert_double(value_holder, size_info)

        # If dict only contains "#text", return just the value
        if "#text" in cleaned_data and len(cleaned_data) == 1:
            return cleaned_data["#text"]

        return cleaned_data

    elif isinstance(data, list):
        return [clean_structure(item, fields_to_skip) for item in data]

    return data
    
def custom_json_dump(data, file_path):
    """Writes JSON ensuring arrays stay on a single line while keeping objects formatted."""
    formatted_json = json.dumps(data, indent=2, ensure_ascii=False)

    # Fix arrays: Replace newlines within JSON lists
    formatted_json = re.sub(r"\[\s+([\d.,\s-]+)\s+\]", lambda m: "[" + " ".join(m.group(1).split()) + "]", formatted_json)

    with open(file_path, "w", encoding="utf-8") as file:
        file.write(formatted_json)
    
def xml_to_json(xml_file, json_file, fields_to_skip, append_mode):
    """Converts an XML file to JSON while removing attributes and handling `double` types correctly."""
    if not os.path.exists(xml_file):
        logging.error(f"Input file '{xml_file}' does not exist.")
        return

    try:
        logging.info(f"Reading XML file: {xml_file}")
        with open(xml_file, "r", encoding="utf-8") as file:
            xml_content = file.read()

        logging.info("Parsing XML data...")
        parsed_dict = xmltodict.parse(xml_content)
        cleaned_dict = clean_structure(parsed_dict, fields_to_skip)

        # Handle appending to an existing JSON file
        if append_mode and os.path.exists(json_file):
            logging.info(f"Appending to existing JSON file: {json_file}")
            with open(json_file, "r", encoding="utf-8") as file:
                try:
                    existing_data = json.load(file)
                    if isinstance(existing_data, dict) and isinstance(cleaned_dict, dict):
                        existing_data.update(cleaned_dict)
                    else:
                        existing_data = [existing_data, cleaned_dict]
                except json.JSONDecodeError:
                    logging.warning(f"Existing JSON file '{json_file}' is invalid. Overwriting instead.")
                    existing_data = cleaned_dict
        else:
            existing_data = cleaned_dict

        with open(json_file, "w", encoding="utf-8") as file:
            custom_json_dump(existing_data, json_file)

        logging.info(f"Conversion successful! JSON saved to: {json_file}")

    except Exception as e:
        logging.error(f"Error processing file: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert XML to JSON while removing attributes and filtering fields.")
    parser.add_argument("xml_file", help="Path to the input XML file")
    parser.add_argument("json_file", help="Path to the output JSON file")
    parser.add_argument("--skip", nargs="*", default=[], help="List of XML fields to remove (space-separated)")
    parser.add_argument("--append", action="store_true", help="Append to an existing JSON file instead of overwriting")

    args = parser.parse_args()
    xml_to_json(args.xml_file, args.json_file, set(args.skip), args.append)
