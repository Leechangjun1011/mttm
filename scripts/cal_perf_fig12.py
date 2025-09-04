import argparse

def calculate_time(value1, value2):
    try:
        # Convert values to floats for division
        local = float(value1) #local
        target = float(value2) #target
        
        # Check for division by zero
        if local == 0:
            raise ValueError("Error: Division by zero is not allowed.")
            
        result = (target / local) - 1
        return result
    except ValueError as e:
        return f"Error: {e}"

def calculate_throughput(value1, value2):
    try:
        # Convert values to floats for division
        local = float(value1) #local
        target = float(value2) #target
        
        # Check for division by zero
        if local == 0:
            raise ValueError("Error: Division by zero is not allowed.")
            
        result = 1- (target / local)
        return result
    except ValueError as e:
        return f"Error: {e}"


def main():
    parser = argparse.ArgumentParser(description="Perform various calculations based on specified operations.")

    # A single positional argument to capture the entire list of inputs.
    parser.add_argument(
        'args',
        nargs='+', # 'nargs="+"' means one or more arguments.
        help="A series of operations and their values, e.g., PR 10 5 BC 4 2."
    )

    args = parser.parse_args()

    # Define a dictionary to map operation names to their calculation functions.
    operations = {
        'PR': calculate_time,
        'BC': calculate_time,
        'xsbench': calculate_time,
        'fotonik': calculate_time,
        'dlrm': calculate_throughput,
        'silo': calculate_throughput,
        'xindex': calculate_throughput,
        'btree': calculate_time,
	'average': None,
    }



    if len(args.args) == 3:
        op = args.args[0]
        val1 = args.args[1]
        val2 = args.args[2]

        # Check if the operation is valid and has a corresponding function.
        if op in operations and operations[op] is not None:
            result = operations[op](val1, val2)
            print(f"{result*100:.2f} %")
        else:
            print(f"Warning: Operation '{op}' is not supported or not implemented yet.")
    else:
        print("Error: Incomplete argument set. Each operation requires two values.")
        


if __name__ == "__main__":
    main()
