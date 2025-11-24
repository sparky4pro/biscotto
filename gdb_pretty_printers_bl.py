# gdb_pretty_printers.py
import gdb

# ---------------------------
# Pretty printer for 'string'
# ---------------------------
class StringPrinter:
    "Prints custom 'string' type"
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            length = int(self.val['len'])
            ptr = self.val['ptr']
            return ptr.string(length=length)
        except Exception:
            return "<invalid or corrupted string>"

    def children(self):
        yield ("[len]", self.val['len'])
        yield ("[ptr]", self.val['ptr'])

# ---------------------------
# Pretty printer for 'da.*'
# ---------------------------
class DaPrinter:
    "Prints dynamic array 'da.*'"
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            length = int(self.val['len'])
            allocated = int(self.val['allocated_elems'])
            return f"[{length}/{allocated}]"
        except Exception:
            return "<da print error>"

    def children(self):
        yield ("[len]", self.val['len'])
        yield ("[allocated_elems]", self.val['allocated_elems'])
        # iterate over elements
        ptr = self.val['ptr']
        length = int(self.val['len'])
        for i in range(length):
            yield (f"[{i}]", ptr[i])

# ---------------------------
# Pretty printer for 'sl.*'
# ---------------------------
class SlPrinter:
    "Prints slice 'sl.*'"
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            length = int(self.val['len'])
            return f"[{length}]"
        except Exception:
            return "<da print error>"

    def children(self):
        yield ("[len]", self.val['len'])
        # iterate over elements
        ptr = self.val['ptr']
        length = int(self.val['len'])
        for i in range(length):
            yield (f"[{i}]", ptr[i])

# ---------------------------
# Pretty printer for hash table: 's.{...}' type
# ---------------------------
class HashTablePrinter:
    "Prints hash table type"
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            length = int(self.val['len'])
            slots_len = int(self.val['slots']['len'])
            return f"[{length}/{slots_len}] Hash Table"
        except Exception:
            return "<hash table print error>"

    def children(self):
        try:
            length = int(self.val['len'])
            keys_ptr = self.val['keys']['ptr']
            values_ptr = self.val['values']['ptr']
            for i in range(length):
                key = keys_ptr[i]
                value = values_ptr[i]
                yield (f"[Key = {key}]", value)
        except Exception:
            return
            yield  # empty generator

# ---------------------------
# Register printers
# ---------------------------
def lookup_function(val):
    type_str = str(val.type)
    if type_str == "struct string":
        return StringPrinter(val)
    if type_str == "struct sl.{s64,p.u8}":
        return StringPrinter(val)
    if type_str.startswith("struct da."):
        return DaPrinter(val)
    if type_str.startswith("struct sl."):
        return SlPrinter(val)
    if type_str.startswith("struct s.{sl.{s64,p.s."):
        return HashTablePrinter(val)
    return None

gdb.pretty_printers.append(lookup_function)


