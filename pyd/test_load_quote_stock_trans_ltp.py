
# -*- coding: utf-8 -*-

# 函数说明：SplitQuoteStockTransLTP
#
# 函数：result = SplitQuote(file_path, file_head, file_code, quote_list, list_info, dict_file_sh, dict_file_sz)
#
# 功能: 将每日落地转储的行情数据切割生成单独的证券行情数据文件。
#
# 入参：file_path    <- 落地转储文件路径。
#　 　　file_head    <- 落地转储文件头部字节长度。
#　 　　file_code    <- 落地转储文件数据压缩格式，未压缩则置空字符串。
#　 　　quote_list   <- 需要筛选的证券列表，英文逗号分隔，为空表示全市场，注意字母大小写。
#
# 返回：result       -> 函数调用执行结果，失败返回：-1，成功返回：>= 0。
#　 　　list_info    -> 函数调用执行信息。
#　 　　dict_file_sh -> 上交所证券索引与该证券行情数据文件路径。
#　 　　dict_file_sz -> 深交所证券索引与该证券行情数据文件路径。

# 函数说明
#
# 函数：result = LoadQuote(file_path, file_head, file_code, data_code, quote_list, list_info, dict_data)
#
# 功能: 从落地转储文件导入LTP实时逐笔成交行情数据。
#
# 入参：file_path  <- 落地转储文件路径。
#　 　　file_head  <- 落地转储文件头部字节长度。
#　 　　file_code  <- 落地转储文件数据压缩格式，未压缩则置空字符串。
#　 　　data_code  <- 落地转储行情数据字符编码，utf8 或 gbk 等。
#　 　　quote_list <- 需要筛选的合约列表，英文逗号分隔，为空表示全市场，注意字母大小写。
#
# 返回：result     -> 函数调用执行结果，失败返回：-1，成功返回：>= 0。
#　 　　list_info  -> 函数调用执行信息。
#　 　　dict_data  -> LTP实时逐笔成交行情数据。
#
# 数据："Code", "Name", "Type", "Market", "Index", "Price", "Volume", "Turnover", 
#　 　　"TransTime", "LocalTime", "LocalIndex"。
#　 　　具体行情数据格式请参考相关业务文档。

import LoadQuoteStockTransLTP
import SplitQuoteStockTransLTP

file_head = 32
file_code = ""
data_code = "gbk"
file_path = "F:\\201810\\20181031.tr"

list_info = []
dict_data = {}

dict_file_sh = {}
dict_file_sz = {}

def PrintQuoteData(result, list_info, dict_data):
    print(result, len(list_info))
    if result >= 0:
        for i in range(result):
            print(dict_data["Code"][i], dict_data["Name"][i], dict_data["Type"][i], dict_data["Market"][i], \
                  dict_data["Index"][i], dict_data["Price"][i], dict_data["Volume"][i], dict_data["Turnover"][i], \
                  dict_data["TransTime"][i], dict_data["LocalTime"][i], dict_data["LocalIndex"][i])
        for i in range(len(list_info)):
            print("成功:", list_info[i])
    else:
        for i in range(len(list_info)):
            print("失败:", list_info[i])

#result = SplitQuoteStockTransLTP.SplitQuote(file_path, file_head, file_code, "", list_info, dict_file_sh, dict_file_sz)
result = SplitQuoteStockTransLTP.SplitQuote(file_path, file_head, file_code, "600000, 000001, 600004, 000005", list_info, dict_file_sh, dict_file_sz)
print(result, len(list_info))
if result >= 0:
    for code, file in dict_file_sh.items():
        print("SH:", code, file)
    if len(dict_file_sh) > 0 and "600000" in dict_file_sh.keys():
        result = LoadQuoteStockTransLTP.LoadQuote(dict_file_sh["600000"], file_head, file_code, data_code, "", list_info, dict_data)
        PrintQuoteData(result, list_info, dict_data)
    for code, file in dict_file_sz.items():
        print("SZ:", code, file)
    if len(dict_file_sz) > 0 and "000001" in dict_file_sz.keys():
        result = LoadQuoteStockTransLTP.LoadQuote(dict_file_sz["000001"], file_head, file_code, data_code, "", list_info, dict_data)
        PrintQuoteData(result, list_info, dict_data)
    for i in range(len(list_info)):
        print("成功:", list_info[i])
else:
    for i in range(len(list_info)):
        print("失败:", list_info[i])

#result = LoadQuoteStockTransLTP.LoadQuote(file_path, file_head, file_code, data_code, "", list_info, dict_data) # 注意内存哦
result = LoadQuoteStockTransLTP.LoadQuote(file_path, file_head, file_code, data_code, "600000, 000001, 600004, 000005", list_info, dict_data)
PrintQuoteData(result, list_info, dict_data)
