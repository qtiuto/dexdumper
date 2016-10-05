//
// Created by Karven on 2016/9/30.
//

#ifndef HOOKMANAGER_VERFIYERROR_H
#define HOOKMANAGER_VERFIYERROR_H
enum VerifyError {
    VERIFY_ERROR_NONE = 0, /* no error; must be zero */
            VERIFY_ERROR_GENERIC, /* VerifyError */

            VERIFY_ERROR_NO_CLASS, /* NoClassDefFoundError */
            VERIFY_ERROR_NO_FIELD, /* NoSuchFieldError */
            VERIFY_ERROR_NO_METHOD, /* NoSuchMethodError */
            VERIFY_ERROR_ACCESS_CLASS, /* IllegalAccessError */
            VERIFY_ERROR_ACCESS_FIELD, /* IllegalAccessError */
            VERIFY_ERROR_ACCESS_METHOD, /* IllegalAccessError */
            VERIFY_ERROR_CLASS_CHANGE, /* IncompatibleClassChangeError */
            VERIFY_ERROR_INSTANTIATION, /* InstantiationError */
};

/*
 * Identifies the type of reference in the instruction that generated the
 * verify error (e.g. VERIFY_ERROR_ACCESS_CLASS could come from a method,
 * field, or class reference).
 *
 * This must fit in two bits.
 */
enum VerifyErrorRefType {
    VERIFY_ERROR_REF_CLASS = 0,
    VERIFY_ERROR_REF_FIELD = 1,
    VERIFY_ERROR_REF_METHOD = 2,
};
#endif //HOOKMANAGER_VERFIYERROR_H
