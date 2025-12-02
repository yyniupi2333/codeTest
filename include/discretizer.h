#ifndef DISCRETIZER_H
#define DISCRETIZER_H

#include <stdbool.h>
#include <limits.h>

#define MAX_GROUPS 64
#define INITIAL_SAMPLE_WINDOW 8
#define EXPANSION_FACTOR 2
#define MIN_SPAN 1
#define MIN_COUNT 2
#define THRESHOLD_RATIO 2

/* ============ 数据结构定义 ============ */

// 组结构体
typedef struct Group {
    int group_id;       // 组编号(0-63)
    long long min;      // 组内最小值
    long long max;      // 组内最大值
    int count;          // 组内数据个数
    bool active;        // 组是否活跃（叶子节点）
} Group;

// 二叉树节点结构
typedef struct TreeNode {
    long long split_value;     // 分裂值（内部节点使用）
    struct TreeNode* left;     // 左子树
    struct TreeNode* right;    // 右子树
    struct TreeNode* parent;   // 父节点指针，用于边界调整
    Group* group;              // 叶子节点关联的组
} TreeNode;

// 离散化管理器
typedef struct {
    TreeNode* root;            // 二叉树根节点
    Group groups[MAX_GROUPS];  // 所有组
    int group_count;           // 当前组数
    int next_group_id;         // 下一个可用的组ID
    bool initialized;          // 是否已根据第一个值初始化
} Discretizer;

/* ============ 函数声明 ============ */

// 初始化函数
void init_discretizer(Discretizer* disc, long long initial_min, long long initial_max);
void initialize_with_first_value(Discretizer* disc, long long first_value);
void init_group(Group* group, int group_id);

// 树节点操作
TreeNode* create_tree_node(long long split_value, Group* group);
void free_tree(TreeNode* node);

// 叶子节点查找
TreeNode* find_leftmost_leaf(TreeNode* node);
TreeNode* find_rightmost_leaf(TreeNode* node);
TreeNode* get_sibling(TreeNode* node);

// 核心操作
bool need_split(const Group* group);
long long calculate_split_point(long long min, long long max);
bool split_group(Discretizer* disc, TreeNode* parent);
bool adjust_boundary(Discretizer* disc, TreeNode* edge_node, bool is_left_edge);

// 查询和更新
int query_and_update(Discretizer* disc, long long value);
int find_group_id(const Discretizer* disc, long long value);

// 工具函数
void print_groups(const Discretizer* disc);

#endif // DISCRETIZER_H
