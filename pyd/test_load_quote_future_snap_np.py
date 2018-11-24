
# -*- coding: utf-8 -*-

# 函数说明
#
# 函数：result = LoadQuote(file_path, file_head, file_code, data_code, quote_list, list_info, dict_data)
#
# 功能: 从落地转储文件导入实时内盘期货快照行情数据。
#
# 入参：file_path  <- 落地转储文件路径。
#　 　　file_head  <- 落地转储文件头部字节长度。
#　 　　file_code  <- 落地转储文件数据压缩格式，未压缩则置空字符串。
#　 　　data_code  <- 落地转储行情数据字符编码，utf8 或 gbk 等。
#　 　　quote_list <- 需要筛选的合约列表，英文逗号分隔，为空表示全市场，注意字母大小写。
#
# 返回：result     -> 函数调用执行结果，失败返回：-1，成功返回：>= 0。
#　 　　list_info  -> 函数调用执行信息。
#　 　　dict_data  -> 实时内盘期货快照行情数据。
#
# 数据："Code", "Name", "Type", "Market", "Status", 
#　 　　"Last", "Open", "High", "Low", "Close", "PreClose", "Volume", "Turnover", 
#　 　　"AskPrice[5]", "AskVolume[5]", "BidPrice[5]", "BidVolume[5]", 
#　 　　"HighLimit", "LowLimit", "Settle", "PreSettle", "Position", "PrePosition", 
#　 　　"Average", "UpDown", "UpDownRate", "Swing", "Delta", "PreDelta", 
#　 　　"QuoteDate", "QuoteTime", "LocalDate", "LocalTime", "LocalIndex"。
#　 　　具体行情数据格式请参考相关业务文档。

import LoadQuoteFutureSnapNP

file_head = 32
file_code = "lzma"
data_code = "utf8"
file_path = "C:\\Users\\xrd\\Desktop\\20160222.hq.lzma"

list_info = []
dict_data = {}

#result = LoadQuoteFutureSnapNP.LoadQuote(file_path, file_head, file_code, data_code, "", list_info, dict_data) # 注意内存哦
result = LoadQuoteFutureSnapNP.LoadQuote(file_path, file_head, file_code, data_code, "au1606", list_info, dict_data)
#result = LoadQuoteFutureSnapNP.LoadQuote(file_path, file_head, file_code, data_code, "IF1603, ag1606, au1606", list_info, dict_data)
print(result, len(list_info))
if result >= 0:
    for i in range(result):
        print(dict_data["Code"][i], dict_data["Type"][i], dict_data["Status"][i], dict_data["Last"][i], \
              dict_data["AskPrice"][0][i], dict_data["AskVolume"][0][i], dict_data["BidPrice"][0][i], dict_data["BidVolume"][0][i], \
              dict_data["QuoteDate"][i], dict_data["QuoteTime"][i], dict_data["LocalIndex"][i])
    for i in range(len(list_info)):
        print("成功：", list_info[i])
else:
    for i in range(len(list_info)):
        print("失败：", list_info[i])
