import chardet

def print_byte_utf8_to_cn(byte_str):
    byte_str = str(byte_str, encoding="utf8")
    print(byte_str)
    return

def print_byte_gbk_to_cn(byte_str):
    byte_str = str(byte_str, encoding="gbk")
    print(byte_str)
    return

# utf-8
uyf8_byte_str = b"\346\224\266\350\227\217\345\244\271"
print_byte_utf8_to_cn(uyf8_byte_str)

# gbk
gbk_byte_str = b"\261\261\276\251\312\320-\266\253\263\307\307\370-\276\260\311\275\307\260\275\326\064\272\305"
print_byte_gbk_to_cn(gbk_byte_str)