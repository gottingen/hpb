

@export
def dict_test(d: Dict[str, int]):
    for k, v in d.items():
        print(k, v)

dict_test({'a': 1, 'b': 2, 'c': 3, 'd': 4, 'e': 5})