#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>

#define MAX_GROUPS 64

// 组结构体
typedef struct Group {
    int group_id;       // 组编号(0-63)
    double min;         // 组内最小值
    double max;         // 组内最大值
    int count;          // 组内数据个数
    bool active;        // 组是否活跃（叶子节点）
} Group;

// 二叉树节点结构
typedef struct TreeNode {
    double split_value;     // 分裂值（内部节点使用）
    struct TreeNode* left;  // 左子树
    struct TreeNode* right; // 右子树
    Group* group;          // 叶子节点关联的组
} TreeNode;

// 离散化管理器
typedef struct {
    TreeNode* root;         // 二叉树根节点
    Group groups[MAX_GROUPS]; // 所有组
    int group_count;        // 当前组数
    int next_group_id;      // 下一个可用的组ID
} Discretizer;

// 初始化组
void init_group(Group* group, int group_id) {
    group->group_id = group_id;
    group->min = HUGE_VAL;
    group->max = -HUGE_VAL;
    group->count = 0;
    group->active = true;
}

// 创建新树节点
TreeNode* create_tree_node(double split_value, Group* group) {
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->split_value = split_value;
    node->left = NULL;
    node->right = NULL;
    node->group = group;
    return node;
}

// 检查组是否需要分裂
bool need_split(const Group* group) {
    if (group->count < 2) return false;
    return group->max >= 2 * group->min;
}

// 计算分裂点（使用几何平均数，更适合比值条件）
double calculate_split_point(double min, double max) {
    return sqrt(min * max); // 几何平均
}

// 分裂组
bool split_group(Discretizer* disc, TreeNode* parent) {
    if (disc->group_count >= MAX_GROUPS || parent->group == NULL) {
        return false;
    }
    
    Group* parent_group = parent->group;
    
    // 计算分裂点
    double split_point = calculate_split_point(parent_group->min, parent_group->max);
    
    // 检查是否有足够的空间创建新组
    if (disc->next_group_id + 1 >= MAX_GROUPS) {
        return false;
    }
    
    // 创建两个新组
    Group* left_group = &disc->groups[disc->next_group_id];
    Group* right_group = &disc->groups[disc->next_group_id + 1];
    
    init_group(left_group, disc->next_group_id);
    init_group(right_group, disc->next_group_id + 1);
    
    // 设置新组的范围
    left_group->min = parent_group->min;
    left_group->max = split_point;
    right_group->min = split_point;
    right_group->max = parent_group->max;
    
    // 创建子树节点
    parent->left = create_tree_node(0, left_group);
    parent->right = create_tree_node(0, right_group);
    parent->split_value = split_point;
    
    // 父节点不再是叶子节点
    parent->group->active = false;
    
    // 更新组计数（分裂后增加一个组）
    disc->group_count += 1;
    disc->next_group_id += 2;
    
    printf("分裂组 %d -> [%d, %d], 分裂点: %.6f\n", 
           parent_group->group_id, left_group->group_id, right_group->group_id, split_point);
    
    return true;
}

// 合并的查询和更新函数：查找值所属的组ID，同时更新该组的统计信息
int query_and_update(Discretizer* disc, double value) {
    TreeNode* current = disc->root;
    TreeNode* parent = NULL;
    
    // 遍历二叉树找到合适的叶子节点（组）
    while (current != NULL) {
        // 如果是叶子节点且活跃
        if (current->group != NULL && current->group->active) {
            Group* group = current->group;
            int group_id = group->group_id;
        
            // 更新组的统计信息
            if (group->count == 0) {
                // 第一个数据
                group->min = value;
                group->max = value;
            } else {
                if (value < group->min) group->min = value;
                if (value > group->max) group->max = value;
            }
            group->count++;
            
            // 检查是否需要分裂
            if (need_split(group) && disc->group_count < MAX_GROUPS) {
                split_group(disc, current);
            }
            
            return group_id;
        } else {
            // 内部节点：根据分裂值决定方向
            parent = current;
            if (value < current->split_value) {
                current = current->left;
            } else {
                current = current->right;
            }
        }
    }
    
    return -1; // 未找到合适的组
}

// 初始化离散化器
void init_discretizer(Discretizer* disc, double initial_min, double initial_max) {
    disc->group_count = 1;
    disc->next_group_id = 1;
    
    // 初始化第一个组
    init_group(&disc->groups[0], 0);
    disc->groups[0].min = initial_min;
    disc->groups[0].max = initial_max;
    
    // 创建根节点
    disc->root = create_tree_node(0, &disc->groups[0]);
}

// 打印所有组信息
void print_groups(const Discretizer* disc) {
    printf("\n当前组数: %d/%d\n", disc->group_count, MAX_GROUPS);
    printf("各组详细信息:\n");
    
    int active_count = 0;
    for (int i = 0; i < disc->next_group_id; i++) {
        if (disc->groups[i].active) {
            const Group* g = &disc->groups[i];
            bool condition_met = (g->count == 0) || (g->max < 2 * g->min);
            printf("组 %2d: min=%10.6f, max=%10.6f, 数据量=%3d, 条件满足=%s\n", 
                   g->group_id, g->min, g->max, g->count, 
                   condition_met ? "是" : "否");
            active_count++;
        }
    }
    printf("活跃组总数: %d\n", active_count);
}

// 释放二叉树内存（递归）
void free_tree(TreeNode* node) {
    if (node == NULL) return;
    
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

// 释放离散化器资源
void free_discretizer(Discretizer* disc) {
    if (disc != NULL) {
        free_tree(disc->root);
    }
}
int main() {
    Discretizer disc;
    
    // 初始化，假设数据范围大致在[1, 100000]之间
    init_discretizer(&disc, 1.0, 100000.0);
    
    // 生成测试数据（近似指数分布，容易触发分裂）
    double test_data[] = {
        1.0, 2.0, 3.0, 5.0, 8.0, 13.0, 21.0, 34.0, 55.0, 89.0,
        144.0, 233.0, 377.0, 610.0, 987.0, 1597.0, 2584.0, 4181.0,
        6765.0, 10946.0, 17711.0, 28657.0, 46368.0, 50000.0, 75000.0
    };
    
    int data_count = sizeof(test_data) / sizeof(test_data[0]);
    
    printf("开始离散化处理...\n");
    printf("目标组数: %d, 每组需满足 max < 2 * min\n", MAX_GROUPS);
    
    // 处理流式数据
    for (int i = 0; i < data_count; i++) {
        if (disc.group_count >= MAX_GROUPS) {
            printf("已达到最大组数 %d，停止处理新数据\n", MAX_GROUPS);
            break;
        }
        
        int assigned_group = query_and_update(&disc, test_data[i]);
        printf("处理值 %8.1f -> 组 %d, 当前组数: %d\n", 
               test_data[i], assigned_group, disc.group_count);
    }
    
    // 打印最终结果
    print_groups(&disc);
    
    // 测试查询功能（不更新统计信息）
    printf("\n测试查询功能:\n");
    double test_values[] = {10.0, 100.0, 1000.0, 10000.0, 50000.0, 90000.0};
    for (int i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        // 注意：这里使用query_and_update会更新统计信息
        // 如果只想查询而不更新，可以创建一个只读版本的函数
        int group_id = query_and_update(&disc, test_values[i]);
        printf("值 %8.1f 属于组 %d\n", test_values[i], group_id);
    }
    
    // 释放资源
    free_discretizer(&disc);
    
    return 0;
}