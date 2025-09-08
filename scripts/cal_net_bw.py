import sys
import numpy as np

def get_bw(file_path):
    """
    Reads a file line by line, and calculates the average, min, and max of the numbers.

    Args:
        file_path (str): The path to the file to be processed.

    Returns:
        A tuple containing the average, minimum, and maximum values, or None if an error occurs.
    """
    numbers = []

    try:
        with open(file_path, 'r') as file:
            for line in file:
                # Strip whitespace and check if the line is not empty
                clean_line = line.strip()
                if not clean_line:
                    continue  # Skip empty lines

                try:
                    # Convert the line to a float and add it to our list
                    numbers.append(float(clean_line))
                except ValueError:
                    print(f"Warning: Skipping invalid number on line: '{line.strip()}'")
                    continue

    except FileNotFoundError:
        print(f"Error: The file '{file_path}' was not found.")
        return None
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return None

    # Check if we have any numbers to process
    if not numbers:
        print("No valid numbers were found in the file.")
        return None


    return numbers

def main():
    """
    Main function to run the script from the command line.
    It expects one argument: the path to the data file.
    """
    # Check for the correct number of command-line arguments
    if len(sys.argv) != 3:
        print("Usage: python script_name.py <file_path1> <file_path2>")
        sys.exit(1)

    file_path_1 = sys.argv[1]
    file_path_2 = sys.argv[2]

    local_bw = get_bw(file_path_1)
    remote_bw = get_bw(file_path_2)

    tot_bw = np.array(local_bw) + np.array(remote_bw)
    net_bw = [i - 40000 for i in tot_bw]

    for i in net_bw:
        print(i)

if __name__ == "__main__":
    main()
