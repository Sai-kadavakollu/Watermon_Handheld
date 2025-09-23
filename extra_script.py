Import("env")

def ConvertListoDict(lst):
    res_dct = {lst[i]: lst[i + 1] for i in range(0, len(lst), 2)}
    return res_dct

my_flags = env.ParseFlags(env['BUILD_FLAGS'])
flattrn = [val for sublist in my_flags.get("CPPDEFINES") for val in sublist]

dist = ConvertListoDict(flattrn)

env.Replace(PROGNAME="%s" % dist.get("APP_NAME"))