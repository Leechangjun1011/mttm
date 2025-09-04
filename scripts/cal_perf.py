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

    avg_perf = 0

    if args.args[0] == 'average':
        if len(args.args) == 5:
            for i in range(4):
                avg_perf += float(args.args[i+1])
            avg_perf /= 4
            print(avg_perf)
        else:
            print("Error: average")
        return


    for i in range(0, len(args.args), 3):
        # Ensure we have a complete set of operation and two values.
        if i + 2 < len(args.args):
            op = args.args[i]
            val1 = args.args[i+1]
            val2 = args.args[i+2]

            # Check if the operation is valid and has a corresponding function.
            if op in operations and operations[op] is not None:
                result = operations[op](val1, val2)
                avg_perf = avg_perf + result
                #print(f"'{op}' with local '{val1}' target '{val2}' resulted in: {result}")
            else:
                print(f"Warning: Operation '{op}' is not supported or not implemented yet.")
        else:
            print("Error: Incomplete argument set. Each operation requires two values.")
            break

    avg_perf = avg_perf / 3
    print(avg_perf * 100)

if __name__ == "__main__":
    main()
