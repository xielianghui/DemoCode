import chardet

def utf8tocn(byte_str):
    byte_str = str(byte_str, encoding="utf-8")
    print("utf-8解码为: " + byte_str)
    return

def gbktocn(byte_str):
    byte_str = str(byte_str, encoding="gbk")
    print("gbk解码为: " + byte_str)
    return


utf8_str = b"\350\200\201\345\271\262\345\246\210"
utf8tocn(utf8_str)

gbk_str = b"\261\261\276\251"
gbktocn(gbk_str)
