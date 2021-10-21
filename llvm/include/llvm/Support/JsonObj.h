#ifndef TRANSFORMS_OBFUSCATION_JSONCLASS_H
#define TRANSFORMS_OBFUSCATION_JSONCLASS_H

namespace llvm {

class JsonRead;

class JsonClass {
public:
    virtual bool json(JsonRead *JsonReadObj) = 0;
    virtual ~JsonClass() {}
};

}

#endif
