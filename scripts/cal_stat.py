import sys

def calculate_stats(file_path):
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

    # Calculate the statistics
    total = sum(numbers)
    count = len(numbers)
    average = total / count
    min_val = min(numbers)
    max_val = max(numbers)


    return (average, min_val, max_val)

def main():
    """
    Main function to run the script from the command line.
    It expects one argument: the path to the data file.
    """
    # Check for the correct number of command-line arguments
    if len(sys.argv) != 3:
        print("Usage: python script_name.py <file_path>")
        sys.exit(1)

    file_path = sys.argv[1]
    stats = calculate_stats(file_path)
    capacity = float(sys.argv[2]) * 1024;

    if stats:
        average, min_val, max_val = stats
        #print(f"capacity: {capacity}")
        print(f"Average: {average:.2f} ({average*100/capacity:.1f} %)")  # Formatted to 2 decimal places
        print(f"Minimum: {min_val} ({min_val*100/capacity:.1f} %)")
        print(f"Maximum: {max_val} ({max_val*100/capacity:.1f} %)")

if __name__ == "__main__":
    main()
