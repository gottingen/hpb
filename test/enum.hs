

class Enum(int):
    A = 1
    B = 2
    C = 3

@export
def enum_test(e: Enum):
    print(e)

enum_test(Enum.A)
