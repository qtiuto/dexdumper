//
// Created by Karven on 2016/9/25.
//

#ifndef HOOKMANAGER_INLINETABLE_H
#define HOOKMANAGER_INLINETABLE_H
struct InlineMethod {
    const char *classDescriptor;
    const char *methodName;
    const char *retType;
    const char *parSig;
    u4 methodIdx;
} InlineOpsTable[] = {
        {"Lorg/apache/harmony/dalvik/NativeTestTarget;", "emptyInlineMethod",   "V", ""},
        {"Ljava/lang/String;",                           "charAt",              "C", "I"},
        {"Ljava/lang/String;",                           "compareTo",           "I", "Ljava/lang/String;"},
        {"Ljava/lang/String;",                           "equals",              "Z", "Ljava/lang/Object;"},
        {"Ljava/lang/String;",                           "fastIndexOf",         "I", "II"},
        {"Ljava/lang/String;",                           "isEmpty",             "Z", ""},
        {"Ljava/lang/String;",                           "length",              "I", ""},

        {"Ljava/lang/Math;",                             "abs",                 "I", "I"},
        {"Ljava/lang/Math;",                             "abs",                 "J", "J"},
        {"Ljava/lang/Math;",                             "abs",                 "F", "F"},
        {"Ljava/lang/Math;",                             "abs",                 "D", "D"},
        {"Ljava/lang/Math;",                             "min",                 "I", "II"},
        {"Ljava/lang/Math;",                             "max",                 "I", "II"},
        {"Ljava/lang/Math;",                             "sqrt",                "D", "D"},
        {"Ljava/lang/Math;",                             "cos",                 "D", "D"},
        {"Ljava/lang/Math;",                             "sin",                 "D", "D"},

        {"Ljava/lang/Float;",                            "floatToIntBits",      "I", "F"},
        {"Ljava/lang/Float;",                            "floatToRawIntBits",   "I", "F"},
        {"Ljava/lang/Float;",                            "intBitsToFloat",      "F", "I"},

        {"Ljava/lang/Double;",                           "doubleToLongBits",    "J", "D"},
        {"Ljava/lang/Double;",                           "doubleToRawLongBits", "J", "D"},
        {"Ljava/lang/Double;",                           "longBitsToDouble",    "D", "J"},

        // These are implemented exactly the same in Math and StrictMath,
        // so we can make the StrictMath calls fast too. Note that this
        // isn't true in general!
        {"Ljava/lang/StrictMath;",                       "abs",                 "I", "I"},
        {"Ljava/lang/StrictMath;",                       "abs",                 "J", "J"},
        {"Ljava/lang/StrictMath;",                       "abs",                 "F", "F"},
        {"Ljava/lang/StrictMath;",                       "abs",                 "D", "D"},
        {"Ljava/lang/StrictMath;",                       "min",                 "I", "II"},
        {"Ljava/lang/StrictMath;",                       "max",                 "I", "II"},
        {"Ljava/lang/StrictMath;",                       "sqrt",                "D", "D"},

};
enum {
    InlineOpsTableSize = 29, //anti editor bug
    InlineVirtualStart = 1,
    InlineVirtualEnd = 6
};

void CodeResolver::resetInlineTable() {
    for (int i = 0; i < InlineOpsTableSize; ++i) {
        InlineOpsTable[i].methodIdx = CodeResolver::UNDEFINED;
    }
}

#endif //HOOKMANAGER_INLINETABLE_H
