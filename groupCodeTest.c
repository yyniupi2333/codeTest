#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

#define MAX_GROUPS 64

// 组结构体
typedef struct Group {
    int group_id;       // 组编号(0-63)
    long long min;      // 组内最小值（改为long long防止溢出）
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
} Discretizer;

// 初始化组
void init_group(Group* group, int group_id) {
    group->group_id = group_id;
    group->min = LLONG_MAX;
    group->max = LLONG_MIN;
    group->count = 0;
    group->active = true;
}

// 创建新树节点
TreeNode* create_tree_node(long long split_value, Group* group) {
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->split_value = split_value;
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->group = group;
    return node;
}

// 检查组是否需要分裂
bool need_split(const Group* group) {
    if (group->count < 2) return false;
    // 使用long long防止整数溢出
    return group->max >= 2 * group->min;
}

// 计算分裂点（整数几何平均近似）
long long calculate_split_point(long long min, long long max) {
    // 处理min为0的情况
    if (min == 0) {
        // 当min为0时，使用算术平均作为fallback
        return min + (max - min) / 2;
    }
    
    // 使用二分法求整数平方根近似（避免浮点数）
    long long left = min;
    long long right = max;
    long long result = min;
    
    while (left <= right) {
        long long mid = left + (right - left) / 2;
        long long square = mid * mid;
        
        if (square <= min * max) {
            result = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    // 确保分裂点在(min, max)范围内
    if (result <= min) result = min + 1;
    if (result >= max) result = max - 1;
    
    return result;
}

// 寻找最左叶子节点
TreeNode* find_leftmost_leaf(TreeNode* node) {
    if (node == NULL) return NULL;
    
    while (node->left != NULL || node->right != NULL) {
        if (node->left != NULL) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return node;
}

// 寻找最右叶子节点
TreeNode* find_rightmost_leaf(TreeNode* node) {
    if (node == NULL) return NULL;
    
    while (node->left != NULL || node->right != NULL) {
        if (node->right != NULL) {
            node = node->right;
        } else {
            node = node->left;
        }
    }
    return node;
}

// 获取兄弟节点
TreeNode* get_sibling(TreeNode* node) {
    if (node == NULL || node->parent == NULL) return NULL;
    
    if (node->parent->left == node) {
        return node->parent->right;
    } else {
        return node->parent->left;
    }
}

// 分裂组
bool split_group(Discretizer* disc, TreeNode* parent) {
    if (disc->group_count >= MAX_GROUPS || parent->group == NULL) {
        return false;
    }
    
    Group* parent_group = parent->group;
    if (!need_split(parent_group)) {
        return false;
    }
    
    // 计算分裂点
    long long split_point = calculate_split_point(parent_group->min, parent_group->max);
    
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
    
    // 设置父指针
    parent->left->parent = parent;
    parent->right->parent = parent;
    
    // 父节点不再是叶子节点
    parent->group->active = false;
    
    // 重新分配父组中的数据到子组（简化：重置计数）
    left_group->count = 0;
    right_group->count = 0;
    
    disc->group_count += 1;
    disc->next_group_id += 2;
    
    printf("分裂组 %d -> [%d, %d], 分裂点: %lld\n", 
           parent_group->group_id, left_group->group_id, right_group->group_id, split_point);
    
    return true;
}

// 边界调整函数
bool adjust_boundary(Discretizer* disc, TreeNode* edge_node, bool is_left_edge) {
    if (edge_node == NULL || edge_node->group == NULL || !edge_node->group->active) {
        return false;
    }
    
    Group* edge_group = edge_node->group;
    
    // 只有达到调整阈值时才进行调整（例如，条件被严重违反）
    if (edge_group->count < 2 || edge_group->max < 2 * edge_group->min * 11 / 10) {
        return false; // 条件违反不严重，不调整
    }
    
    // 找到相邻的兄弟组
    TreeNode* sibling_node = get_sibling(edge_node);
    if (sibling_node == NULL || sibling_node->group == NULL || !sibling_node->group->active) {
        return false;
    }
    
    TreeNode* parent_node = edge_node->parent;
    if (parent_node == NULL) {
        return false;
    }
    
    Group* sibling_group = sibling_node->group;
    
    printf("边界调整: %s边缘组 %d (min=%lld, max=%lld)\n", 
           is_left_edge ? "左" : "右", edge_group->group_id, edge_group->min, edge_group->max);
    
    // 计算新的分裂点
    long long new_split_point = calculate_split_point(edge_group->min, edge_group->max);
    
    // 更新分裂点和组范围
    long long old_split = parent_node->split_value;
    parent_node->split_value = new_split_point;
    
    if (is_left_edge) {
        edge_group->max = new_split_point;
        sibling_group->min = new_split_point;
    } else {
        sibling_group->max = new_split_point;
        edge_group->min = new_split_point;
    }
    
    printf("边界调整完成: 分裂点从 %lld 调整为 %lld\n", old_split, new_split_point);
    
    return true;
}

// 统一的查询和更新函数
int query_and_update(Discretizer* disc, long long value) {
    TreeNode* current = disc->root;
    
    // 遍历二叉树找到合适的叶子节点
    while (current != NULL) {
        if (current->group != NULL && current->group->active) {
            // 到达叶子节点
            Group* group = current->group;
            int group_id = group->group_id;
            
            // 更新组的统计信息
            if (group->count == 0) {
                group->min = value;
                group->max = value;
            } else {
                if (value < group->min) group->min = value;
                if (value > group->max) group->max = value;
            }
            group->count++;
            
            // 检查是否需要分裂或边界调整
            if (disc->group_count < MAX_GROUPS) {
                if (need_split(group)) {
                    split_group(disc, current);
                }
            } else {
                // 达到最大组数，进行边界调整
                TreeNode* leftmost = find_leftmost_leaf(disc->root);
                TreeNode* rightmost = find_rightmost_leaf(disc->root);
                
                if (leftmost != NULL && leftmost->group != NULL) {
                    adjust_boundary(disc, leftmost, true);
                }
                
                if (rightmost != NULL && rightmost->group != NULL) {
                    adjust_boundary(disc, rightmost, false);
                }
            }
            
            return group_id;
        } else {
            // 内部节点：根据分裂值决定方向
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
void init_discretizer(Discretizer* disc, long long initial_min, long long initial_max) {
    disc->group_count = 1;
    disc->next_group_id = 1;
    
    // 初始化第一个组
    init_group(&disc->groups[0], 0);
    disc->groups[0].min = initial_min;
    disc->groups[0].max = initial_max;
    
    // 创建根节点
    disc->root = create_tree_node(0, &disc->groups[0]);
}

// 查询数值所属的组编号（不更新统计信息）
int find_group_id(const Discretizer* disc, long long value) {
    TreeNode* current = disc->root;
    
    while (current != NULL) {
        if (current->group != NULL && current->group->active) {
            return current->group->group_id;
        } else {
            if (value < current->split_value) {
                current = current->left;
            } else {
                current = current->right;
            }
        }
    }
    
    return -1;
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
            printf("组 %2d: min=%12lld, max=%12lld, 数据量=%3d, 条件满足=%s\n", 
                   g->group_id, g->min, g->max, g->count, 
                   condition_met ? "是" : "否");
            active_count++;
        }
    }
    printf("活跃组总数: %d\n", active_count);
}

// 递归释放二叉树内存
void free_tree(TreeNode* node) {
    if (node == NULL) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

// 主函数示例
int main() {
    Discretizer disc;
    
    // 初始化，使用整数范围[0, INT_MAX]
    init_discretizer(&disc, 0, INT_MAX);
    
    // 测试数据（整数序列）
    long long test_data[] = {
        1, 2, 3, 5, 8, 13, 21, 34, 55, 89,
        144, 233, 377, 610, 987, 1597, 2584, 4181,
        6765, 10946, 17711, 28657, 46368, 75025,
        121393, 196418, 317811, 514229, 832040
    };
    
    int data_count = sizeof(test_data) / sizeof(test_data[0]);
    
    printf("开始整数离散化处理...\n");
    printf("目标组数: %d, 每组需满足 max < 2 * min\n", MAX_GROUPS);
    printf("初始范围: [0, %d]\n", INT_MAX);
    
    // 处理数据流
    for (int i = 0; i < data_count; i++) {
        if (disc.group_count >= MAX_GROUPS) {
            printf("已达到最大组数 %d，启用边界调整模式\n", MAX_GROUPS);
            // 继续处理，但只进行边界调整
        }
        
        int assigned_group = query_and_update(&disc, test_data[i]);
        printf("处理值 %8lld -> 组 %d, 当前组数: %d\n", 
               test_data[i], assigned_group, disc.group_count);
    }
    
    // 打印最终结果
    print_groups(&disc);
    
    // 测试查询功能
    printf("\n纯查询测试（不更新统计）:\n");
    long long test_values[] = {10, 100, 1000, 10000, 50000, 100000, 500000};
    for (int i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        int group_id = find_group_id(&disc, test_values[i]);
        printf("值 %8lld 属于组 %d\n", test_values[i], group_id);
    }
    free_tree(disc.root);
    
    return 0;
}