#include "AssociativeOpsTable.h"

using std::map;
using std::string;
using std::vector;

namespace Halide {
namespace Internal {

namespace {

enum class RootExpr {
    Add = 0,
    Mul = 1,
    Max = 2,
    Min = 3,
    Sub = 4,
    Select = 5,
    And = 6,
    Or = 7,
    Cast = 8,
    Unknown = 9, // Not supported IR type
};

enum class ValType {
    UInt1 = 0,
    UInt8 = 1,
    UInt16 = 2,
    UInt32 = 3,
    UInt64 = 4,
    Int8 = 5,
    Int16 = 6,
    Int32 = 7,
    Int64 = 8,
    Float32 = 9,
    Float64 = 10,
    All = 11, // General type (including all previous types)
};

ValType convert_halide_type_to_val_type(const Type &halide_t) {
    internal_assert(halide_t.is_scalar() && !halide_t.is_handle());

    ValType val_t;
    if (halide_t.is_uint()) {
        if (halide_t.bits() == 1) { // Bool
            val_t = ValType::UInt1;
        } else if (halide_t.bits() == 8) {
            val_t = ValType::UInt8;
        } else if (halide_t.bits() == 16) {
            val_t = ValType::UInt16;
        } else if (halide_t.bits() == 32) {
            val_t = ValType::UInt32;
        } else {
            internal_assert(halide_t.bits() == 64);
            val_t = ValType::UInt64;
        }
    } else if (halide_t.is_int()) {
        if (halide_t.bits() == 8) {
            val_t = ValType::Int8;
        } else if (halide_t.bits() == 16) {
            val_t = ValType::UInt16;
        } else if (halide_t.bits() == 32) {
            val_t = ValType::UInt32;
        } else {
            internal_assert(halide_t.bits() == 64);
            val_t = ValType::UInt64;
        }
    } else {
        internal_assert(halide_t.is_float());
        if (halide_t.bits() == 32) {
            val_t = ValType::Float32;
        } else {
            internal_assert(halide_t.bits() == 64);
            val_t = ValType::Float64;
        }
    }
    return val_t;
}

struct TableKey {
    ValType type;
    RootExpr root;
    size_t dim;
    TableKey(ValType t, RootExpr r, size_t d) : type(t), root(r), dim(d) {}

    bool operator==(const TableKey &other) const {
        return (type == other.type) && (root == other.root) && (dim == other.dim);
    }
    bool operator<(const TableKey &other) const {
        if (type < other.type) {
            return true;
        } else if (type > other.type) {
            return false;
        }
        if (root < other.root) {
            return true;
        } else if (root > other.root) {
            return false;
        }
        return (dim < other.dim);
    }
};

static map<TableKey, vector<AssociativePattern>> pattern_tables;

#define declare_vars(t)                     \
    Expr x0 = Variable::make(t, "x0");      \
    Expr y0 = Variable::make(t, "y0");      \
    Expr x1 = Variable::make(t, "x1");      \
    Expr y1 = Variable::make(t, "y1");      \
    Expr k0 = Variable::make(t, "k0");      \
    Expr zero = make_const(t, 0);           \
    Expr one = make_const(t, 1);            \
    Expr neg_one = make_const(t, -1);       \
    Expr tmax = t.max();                    \
    Expr tmin = t.min();                    \

void populate_ops_table_single_general_add(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {x0 + y0, zero, true},
        {Add::make(max(min(y0, k0), y0), x0), zero, true},
        {Add::make(max(Sub::make(k0, y0), y0), x0), zero, true},
        {Add::make(min(max(y0, k0), y0), x0), zero, true},
        {Add::make(min(Sub::make(k0, y0), y0), x0), zero, true},
        {Add::make(max(min(min(y0, k0), y0), y0), x0), zero, true},
        {Add::make(max(min(Mul::make(x0, y0), y0), y0), x0), zero, true},
        {Add::make(max(min(Sub::make(x0, y0), y0), y0), x0), zero, true},
        {Add::make(max(min(Sub::make(y0, x0), y0), y0), x0), zero, true},
        {Add::make(min(max(max(y0, k0), y0), y0), x0), zero, true},
        {Add::make(min(max(Mul::make(x0, y0), y0), y0), x0), zero, true},
        {Add::make(min(max(Sub::make(x0, y0), y0), y0), x0), zero, true},
        {Add::make(min(max(Sub::make(y0, x0), y0), y0), x0), zero, true},
        {Add::make(min(Sub::make(y0, x0), k0), max(y0, x0)), neg_one, true},
        {Add::make(min(y0, x0), max(Sub::make(y0, x0), k0)), zero, true},
    };
}

void populate_ops_table_single_general_mul(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {x0 * y0, one, true},
        {Mul::make(max(min(Mul::make(x0, y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Sub::make(x0, y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Sub::make(y0, x0), y0), y0), x0), one, true},
        {Mul::make(min(max(Mul::make(x0, y0), y0), y0), x0), one, true},
        {Mul::make(min(max(Sub::make(x0, y0), y0), y0), x0), one, true},
        {Mul::make(min(max(Sub::make(y0, x0), y0), y0), x0), one, true},
        {Mul::make(Sub::make(max(min(x0, k0), y0), y0), x0), neg_one, true},
        {Mul::make(Sub::make(min(max(x0, k0), y0), y0), x0), neg_one, true},
        {Mul::make(max(min(Add::make(max(x0, k0), y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Add::make(min(x0, k0), y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Add::make(min(y0, x0), k0), y0), y0), x0), one, true},
        {Mul::make(max(min(Add::make(Mul::make(x0, k0), y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Add::make(Mul::make(x0, y0), y0), y0), y0), x0), one, true},
        {Mul::make(max(min(Add::make(y0, Mul::make(x0, k0)), y0), y0), x0), one, true},
    };
}

void populate_ops_table_single_general_max(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {max(x0, y0), tmin, true},
        {max(min(x0, k0), y0), tmin, true},
        {max(min(y0, x0), y0), tmin, true},
        {max(min(y0, x0), y0), zero, true},
        {max(min(y0, x0) + y0, y0), zero, true},
        {max(min(Add::make(y0, x0), y0), y0), zero, true},
        {max(min(max(y0, k0), x0), y0), tmin, true},
        {max(min(max(y0, k0), y0), x0), tmin, true},
        {max(min(max(y0, x0), k0), y0), tmin, true},
        {max(min(max(y0, x0), y0), y0), tmin, true},
        {max(min(max(y0, x0), y0), y0), zero, true},
        {max(min(min(y0, k0), x0), y0), zero, true},
        {max(min(x0 * y0, y0), y0), zero, true},
    };
}

void populate_ops_table_single_general_min(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {min(x0, y0), tmax, true},
        {min(max(x0, k0), y0), tmax, true},
        {min(max(y0, x0), y0), zero, true},
        {min(max(y0, x0), y0), tmax, true},
        {min(Add::make(max(y0, x0), y0), y0), zero, true},
        {min(max(Add::make(y0, x0), y0), y0), zero, true},
        {min(max(max(y0, k0), x0), y0), zero, true},
        {min(max(min(y0, k0), x0), y0), tmax, true},
        {min(max(min(y0, k0), y0), x0), tmax, true},
        {min(max(min(y0, x0), k0), y0), tmax, true},
        {min(max(min(y0, x0), y0), y0), zero, true},
        {min(max(min(y0, x0), y0), y0), tmax, true},
        {min(max(Mul::make(x0, y0), y0), y0), zero, true},
        {min(max(Mul::make(y0, x0), y0), y0), zero, true},
        {min(max(Sub::make(k0, y0), x0), y0), zero, true},
    };
}

void populate_ops_table_single_general_sub(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {Sub::make(Add::make(max(y0, x0), y0), max(x0, k0)), tmin, true},
        {Sub::make(Add::make(min(y0, x0), y0), min(x0, k0)), tmax, true},
        {Sub::make(max(Add::make(y0, x0), k0), max(y0, x0)), neg_one, true},
        {Sub::make(max(y0, x0), max(Sub::make(x0, y0), k0)), zero, true},
        {Sub::make(min(Add::make(y0, x0), k0), min(y0, x0)), one, true},
        {Sub::make(min(y0, x0), min(Sub::make(x0, y0), k0)), zero, true},
        {Sub::make(Add::make(max(min(min(Sub::make(y0, x0), x0), k0), x0), y0), x0), zero, true},
        {Sub::make(Add::make(max(min(x0, y0), k0), max(x0, y0)), max(x0, k0)), tmin, true},
        {Sub::make(Add::make(min(max(max(Sub::make(y0, x0), x0), k0), x0), y0), x0), zero, true},
        {Sub::make(Add::make(min(max(x0, y0), k0), min(x0, y0)), min(x0, k0)), tmax, true},
    };
}

void populate_ops_table_single_general_select(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
    };
}

void populate_ops_table_double_general_add(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {{Add::make(x0, y0), Add::make(x0, y1)}, {zero, zero}, true},
        {{Add::make(x0, y0), Add::make(x1, y0)}, {zero, zero}, true},
        {{Add::make(x0, y1), Add::make(x1, y1)}, {zero, zero}, true},
        {{Add::make(x1, y0), Add::make(x1, y1)}, {zero, zero}, true},
        {{Add::make(x0, y0), Add::make(Mul::make(x0, k0), y1)}, {zero, zero}, true},
        {{Add::make(x0, y0), Add::make(Mul::make(x0, y0), Add::make(y1, x1))}, {zero, zero}, true},
        {{Add::make(x0, y0), max(min(x0, x1), max(x1, y1))}, {zero, tmin}, true},
        {{Add::make(x0, y0), max(min(x0, y1), max(y1, x1))}, {zero, tmin}, true},
        {{Add::make(x0, y0), min(max(x0, x1), min(x1, y1))}, {zero, tmax}, true},
        {{Add::make(x0, y0), min(max(x0, y1), min(y1, x1))}, {zero, tmax}, true},
        {{Add::make(x0, y0), Sub::make(x1, y0)}, {zero, zero}, true},
        {{Add::make(x0, y0), Sub::make(y1, x0)}, {zero, zero}, true},
        {{Add::make(x0, y0), Sub::make(y1, Mul::make(x0, k0))}, {zero, zero}, true},
        {{Add::make(x0, y0), Sub::make(Add::make(y1, x1), Mul::make(x0, y0))}, {zero, zero}, true},
    };
}

void populate_ops_table_double_general_mul(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {{Mul::make(x0, y0), Add::make(Mul::make(x0, y1), x1)}, {one, zero}, true},
        {{Mul::make(x0, y0), Add::make(Mul::make(x1, y0), y1)}, {one, zero}, true},
        {{Mul::make(x0, y0), Add::make(Mul::make(x0, y0), Sub::make(y1, y0))}, {one, zero}, true},
        {{Mul::make(x0, y0), Add::make(Mul::make(x0, y1), Mul::make(x1, y0))}, {one, zero}, true},
        {{Mul::make(x0, y0), Add::make(Mul::make(x1, y0), Add::make(y0, y1))}, {one, neg_one}, true},
        {{Mul::make(x0, y0), Add::make(Mul::make(x1, y0), Sub::make(y1, y0))}, {one, one}, true},
        {{Mul::make(x0, y0), Mul::make(x0, y1)}, {one, zero}, true},
        {{Mul::make(x0, y0), Mul::make(x1, y0)}, {one, zero}, true},
        {{Mul::make(x1, y0), Mul::make(x1, y1)}, {zero, one}, true},
        {{Mul::make(x0, y0), max(min(x0, x1), max(x1, y1))}, {one, tmin}, true},
        {{Mul::make(x0, y0), max(min(x0, y1), max(y1, x1))}, {one, tmin}, true},
        {{Mul::make(x0, y0), min(max(x0, x1), min(x1, y1))}, {one, tmax}, true},
        {{Mul::make(x0, y0), min(max(x0, y1), min(y1, x1))}, {one, tmax}, true},
        {{Mul::make(x0, y0), Sub::make(Add::make(y0, y1), Mul::make(x0, y0))}, {one, zero}, true},
    };
}

void populate_ops_table_double_general_max(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {{max(x0, y0), select(LT::make(y0, x0), x1, y1)}, {tmin, zero}, true},
        {{max(x0, y0), Add::make(max(x0, y0), Sub::make(y1, y0))}, {tmin, zero}, true},
        {{max(x0, y0), Add::make(min(x0, y0), Add::make(y1, x1))}, {tmin, tmin}, true},
        {{max(x0, y0), Add::make(min(x0, y0), Sub::make(x1, y0))}, {tmin, zero}, true},
        {{max(max(min(Mul::make(x0, y0), x0), y0), x0), Add::make(max(Sub::make(x1, x0), y0), max(x0, y0))}, {tmin, zero}, true},
        {{max(max(min(Mul::make(x0, y0), x0), y0), x0), Add::make(min(max(x0, k0), y0), Sub::make(x1, y0))}, {tmin, zero}, true},
        {{max(min(min(Mul::make(x0, y0), x0), k0), x0), Add::make(Mul::make(max(x0, x1), y1), Add::make(x1, y1))}, {zero, zero}, true},
        {{max(min(max(x0, k0), min(k0, x1)), y0), Mul::make(Sub::make(Add::make(max(x1, y1), x1), min(x0, x1)), Add::make(x0, x1))}, {tmin, tmin}, true},
        {{max(x0, y0), max(x0, y1)}, {tmin, zero}, true},
        {{max(x0, y0), max(x1, y0)}, {tmin, zero}, true},
        {{max(x0, y0), max(y0, x1)}, {tmin, zero}, true},
        {{max(x0, y1), max(x1, y1)}, {zero, tmin}, true},
    };
}

void populate_ops_table_double_general_min(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {{min(x0, y0), select(LT::make(x0, y0), x1, y1)}, {tmax, zero}, true},
        {{min(x0, y0), Add::make(max(x0, y0), Add::make(y1, x1))}, {tmax, tmin}, true},
        {{min(x0, y0), Add::make(max(x0, y0), Sub::make(x1, y0))}, {tmax, zero}, true},
        {{min(x0, y0), Add::make(min(x0, y0), Sub::make(y1, y0))}, {tmax, zero}, true},
        {{min(min(max(Mul::make(x0, y0), x0), y0), x0), Add::make(max(min(x0, k0), y0), Sub::make(x1, y0))}, {tmax, zero}, true},
        {{min(min(max(Mul::make(x0, y0), x0), y0), x0), Add::make(min(Sub::make(x1, x0), y0), min(x0, y0))}, {tmax, zero}, true},
        {{min(x0, y0), Mul::make(max(x0, y0), Mul::make(y1, x1))}, {tmax, tmax}, true},
        {{min(x0, y0), max(min(x0, y1), x1)}, {tmax, tmin}, true},
    };
}

void populate_ops_table_double_general_sub(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {{Sub::make(x0, y1), Add::make(x1, y1)}, {zero, zero}, true},
        {{Sub::make(y0, x1), Add::make(x1, y1)}, {zero, zero}, true},
        {{Sub::make(Mul::make(x0, y0), Mul::make(x1, y1)), Add::make(Mul::make(x1, y0), Mul::make(x0, y1))}, {one, zero}, true},
        {{Sub::make(Add::make(x1, y0), max(Sub::make(x1, x0), k0)), Sub::make(Add::make(x1, y1), max(Sub::make(x1, x0), k0))}, {zero, tmax}, true},
        {{Sub::make(Add::make(x1, y0), min(Sub::make(x1, x0), k0)), Sub::make(Add::make(x1, y1), min(Sub::make(x1, x0), k0))}, {zero, tmin}, true},
        {{Sub::make(Add::make(x1, y0), max(Sub::make(x1, x0), k0)), Sub::make(y1, Mul::make(max(Mul::make(x0, x1), x0), Sub::make(x0, x1)))}, {zero, tmax}, true},
        {{Sub::make(Add::make(x1, y0), min(Sub::make(x1, x0), k0)), Sub::make(y1, Mul::make(max(Mul::make(x0, x1), x0), Sub::make(x0, x1)))}, {zero, tmin}, true},
        {{Sub::make(Add::make(x1, y0), min(Sub::make(x1, x0), k0)), Sub::make(y1, Mul::make(min(Mul::make(x0, x1), x0), Sub::make(x0, x1)))}, {zero, tmin}, true},
        {{Sub::make(Add::make(x1, y0), min(Sub::make(x1, x0), k0)), Sub::make(max(x1, y1), Mul::make(min(Mul::make(x0, x1), x0), Add::make(x0, x1)))}, {zero, tmin}, true},
    };
}

void populate_ops_table_double_general_select(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
    };
}

void populate_ops_table_single_uint1_and(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {x0 && y0, one, true},
    };
}

void populate_ops_table_single_uint1_or(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    table = {
        {x0 || y0, zero, true},
    };
}

void populate_ops_table_single_uint8_cast(Type t, vector<AssociativePattern> &table) {
    declare_vars(t);
    Expr k0_uint16 = Variable::make(UInt(16), "k0");
    table = {
        {cast<uint8_t>(min(cast<uint16_t>(x0 + y0), k0_uint16)), zero, true},
    };
}

static const map<TableKey, void(*)(Type, vector<AssociativePattern> &)> val_type_to_populate_luts_fn = {
    {TableKey(ValType::All, RootExpr::Add, 1), &populate_ops_table_single_general_add},
    {TableKey(ValType::All, RootExpr::Mul, 1), &populate_ops_table_single_general_mul},
    {TableKey(ValType::All, RootExpr::Max, 1), &populate_ops_table_single_general_max},
    {TableKey(ValType::All, RootExpr::Min, 1), &populate_ops_table_single_general_min},
    {TableKey(ValType::All, RootExpr::Sub, 1), &populate_ops_table_single_general_sub},
    {TableKey(ValType::All, RootExpr::Select, 1), &populate_ops_table_single_general_select},
    {TableKey(ValType::All, RootExpr::Add, 2), &populate_ops_table_double_general_add},
    {TableKey(ValType::All, RootExpr::Mul, 2), &populate_ops_table_double_general_mul},
    {TableKey(ValType::All, RootExpr::Max, 2), &populate_ops_table_double_general_max},
    {TableKey(ValType::All, RootExpr::Min, 2), &populate_ops_table_double_general_min},
    {TableKey(ValType::All, RootExpr::Sub, 2), &populate_ops_table_double_general_sub},
    {TableKey(ValType::All, RootExpr::Select, 2), &populate_ops_table_double_general_select},

    {TableKey(ValType::UInt1, RootExpr::And, 1), &populate_ops_table_single_uint1_and},
    {TableKey(ValType::UInt1, RootExpr::Or, 1), &populate_ops_table_single_uint1_or},

    {TableKey(ValType::UInt8, RootExpr::Cast, 1), &populate_ops_table_single_uint8_cast},
};

const vector<AssociativePattern> &get_ops_table_helper(Type t, RootExpr root, size_t dim) {
    TableKey gen_key(ValType::All, root, dim);
    TableKey key(convert_halide_type_to_val_type(t), root, dim);

    const auto &table_it = pattern_tables.find(key);
    if (table_it == pattern_tables.end()) { // Populate the table if we haven't done so previously
        vector<AssociativePattern> &table = pattern_tables[key];

        // Populate the general associative op LUT
        const auto &gen_iter = val_type_to_populate_luts_fn.find(gen_key);
        if (gen_iter != val_type_to_populate_luts_fn.end()) {
            gen_iter->second(t, table);
        }

        // Populate the type-specific associative op LUT
        const auto &iter = val_type_to_populate_luts_fn.find(key);
        if (iter != val_type_to_populate_luts_fn.end()) {
            iter->second(t, table);
        }

        return table;
    }
    return table_it->second;
}

} // anonymous namespace

const vector<AssociativePattern> &get_ops_table(const vector<Expr> &exprs) {
    internal_assert(!exprs.empty());

    // Make sure every expr in the list has the same type
    static vector<AssociativePattern> empty;
    for (size_t i = 1; i < exprs.size() - 1; ++i) {
        user_assert(exprs[i-1].type() == exprs[i].type())
            << "Tuple elements have different type. Can't prove associativity\n";
        return empty;
    }
    if (exprs.size() > 2) {
        debug(5) << "Returning empty table\n";
        return empty;
    }

    RootExpr root = RootExpr::Unknown;
    if (exprs[0].as<Halide::Internal::Add>()) {
        debug(5) << "Returning Add root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Add;
    } else if (exprs[0].as<Halide::Internal::Sub>()) {
        debug(5) << "Returning Sub root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Sub;
    } else if (exprs[0].as<Halide::Internal::Mul>()) {
        debug(5) << "Returning Mul root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Mul;
    } else if (exprs[0].as<Halide::Internal::Min>()) {
        debug(5) << "Returning Min root table\n";
        root = RootExpr::Min;
    } else if (exprs[0].as<Halide::Internal::Max>()) {
        debug(5) << "Returning Max root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Max;
    } else if (exprs[0].as<Halide::Internal::Select>()) {
        debug(5) << "Returning Select root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Select;
    } else if (exprs[0].as<Halide::Internal::And>()) {
        debug(5) << "Returning And root table for type " << exprs[0].type() << "\n";
        root = RootExpr::And;
    } else if (exprs[0].as<Halide::Internal::Or>()) {
        debug(5) << "Returning Or root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Or;
    } else if (exprs[0].as<Halide::Internal::Cast>()) {
        debug(5) << "Returning Cast root table for type " << exprs[0].type() << "\n";
        root = RootExpr::Cast;
    }

    if (root != RootExpr::Unknown) {
        const vector<AssociativePattern> &table = get_ops_table_helper(exprs[0].type(), root, exprs.size());
        debug(5) << "\tTable size: " << table.size() << "\n";
        for (const auto &p : table) {
            debug(5) << p << "\n";
        }
        return table;
    }
    debug(5) << "Returning empty table\n";
    return empty;
}

}
}
