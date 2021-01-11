import requests
import urllib
import zlib
import struct
import re

import susvr_response_pb2
import sudata_service_pb2


def print_str_utf8_to_cn(text_str):
    ls = text_str.group().split("\\")[1:]
    ls = [int(i,8) for i in ls]
    #print(strs)
    return bytes(ls).decode("utf-8")

res_proto = susvr_response_pb2.SusvrResponse()
while 1:
    input_str = input("please input query: ")
    # 拼接uri
    uri_str = urllib.request.quote(input_str)
    #host = "http://10.150.201.33:8780"
    #host = "http://10.35.109.11:8999"
    host = "http://10.232.197.54:8999"
    #host = "http://10.150.201.33:8780" # mirror跳转
    #print(uri_str)
    wd = "/su?voice_pkgid=2-207545&scene_version=&qt=sug&wd=" + str(uri_str)
    last = "&cid=76&type=0&st=0&rp_format=pb&highlight_flag=2&l=18&b=%2812944113%2C4845213%3B12944304%2C4845602%29&loc=%2812944205%2C4845408%29&mb=Mi%2010&os=Android29&gk=1&sv=15.4.0&net=1&cpu=INVALID&resid=01&cuid=E2297852C601F320C17C27D936386F4A%7CVNUUMAACT&bduid=&c3_aid=A00-WUCL5BUTZRW2P6RSQB2N73APHCF7ZXGV-VPLSBFME&channel=baidu&oem=baidu&ndid=&gid=&screen=%281080%2C2120%29&dpi=%28440%2C440%29&ver=1&sinan=splWJaoJSzMWRWotjaMbRjOk%2F&co=460%3A00&cpu_abi=armeabi-v7a&phonebrand=Xiaomi&isart=1&zid=qSKAHI3_UITgcPhrU8b1c9Woz9tF5I5ENTy4BE-4DZCwBP3YN8UwCtGg_IJnSa0o7zI2fdU8GZa1UYR9WN7uKhg&abtest=44508%2C44121%2C44440%2C44437%2C44447%2C44478%2C44498%2C44501%2C44463%2C44504%2C44505&ai_mode=2&sub_ai_mode=2&sesid=1601369097&ctm=1601369140.976000&sign=d81cad1b3cd95946b190ebce4d5e171c"
    uri = host + wd + last
    # 特定的uri
    #uri = "http://10.232.197.54:8999/su?highlight_flag=2&loc=%2812948018.467746%2C4845090.833716%29&wd=%E8%A5%BF%E4%BA%8C%E6%97%97&resid=01&qt=sug&cid=131&type=0&screen=750%2C1334&mb=iPhone7%2C2&sv=9.5.0&ctm=1477472824.144000&l=17&b=%2812947310%2C4843814%3B12948736%2C4846351%29&dpi=%28326%2C326%29&cuid=81e65478645a0e01a4f84a18fa5070dd&rp_format=pb&net=3&st=0&os=android&sinan=Nwsugo2gyIB-zQ6zMzbobi%3D%3Dc&channel=1008648b&ver=1"
    # 发送请求，获取应答
    http_head = {"Accept-Encoding": "fxxk"}#, "ABTest": "{\"44478\":\"SUG_CARDS=2\"}"} # not use gzip
    http_res = requests.get(uri, headers = http_head)
    # 获取头部，解压content
    header = http_res.headers
    #print(header)
    #if 'Content-Encoding' in header and 'gzip' in header['Content-Encoding']:
        #body = gzip.decompress(res.content).decode()
    #body = zlib.decompress(res.content, -zlib.MAX_WBITS)
    #res_proto.ParseFromString(bytes(res.text, encoding = "utf8"))
    #print(struct.calcsize(res.content))
    
    # 开始解析
    content = http_res.content
    content_len = int(header["Content-Length"])
    #print(content_len)

    # get rep head length
    content_len -= 4
    fmt = "!i" + str(content_len) + "s"
    head_lenth, content = struct.unpack(fmt, content)

    # get rep head
    content_len -= head_lenth
    fmt = "!"+ str(head_lenth) + "s" + str(content_len) + "s"
    rep_head_str, content = struct.unpack(fmt, content)
    rep_head = sudata_service_pb2.RepHead()
    rep_head.ParseFromString(rep_head_str)

    rep_result_len = rep_head.message_head[0].length
    res_len = rep_head.message_head[1].length

    # get rep result
    content_len -= rep_result_len
    fmt = "!"+ str(rep_result_len) + "s" + str(content_len) + "s"
    rep_result_str, content = struct.unpack(fmt, content)
    rep_result = sudata_service_pb2.Result()
    rep_result.ParseFromString(rep_result_str)

    # get res
    content_len -= res_len
    fmt = "!"+ str(res_len) + "s" + str(content_len) + "s"
    res_str, content = struct.unpack(fmt, content)
    res = susvr_response_pb2.SusvrResponse()
    res.ParseFromString(res_str)

    # utf8中文处理
    res = res.__str__()
    text = re.sub(r'(?:\\\d{3}){3}', print_str_utf8_to_cn, res)
    #print("response:\n" + text)

    # 输出到文件
    path = "./response.txt"
    file_obj = open(path, mode="w")
    file_obj.write(text)
    file_obj.close()
