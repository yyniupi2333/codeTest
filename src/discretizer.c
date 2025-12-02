#include <stdio.h>
#include <stdlib.h>
#include "../include/discretizer.h"

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
    if (group->count < MIN_COUNT) return false;

    // 当最小值大于0时，使用比例判定
    if (group->min > 0) {
        return group->max >= (long long)THRESHOLD_RATIO * group->min;
    } else if (group->min == 0) {
        // 当 min == 0 时，用绝对跨度判断
        return (group->max - group->min >= MIN_SPAN) && (group->max >= THRESHOLD_RATIO);
    } else {
        // min < 0 的情况（理论上不应发生，因为输入非负）
        return false;
    }
}

// 计算分裂点（纯整数二分法求几何平均近似，避免浮点数）
long long calculate_split_point(long long min, long long max) {
    if (min >= max) return min;
    if (min == 0) return (max - min) / 2;  // 当 min 为 0 时用算术中点

    // 简单启发式：使用算术平均作为分裂点
    return (min + max) / 2;
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
    
    // 分裂后新组的数据从后续样本开始计数
    left_group->count = 0;
    right_group->count = 0;
    
    disc->group_count += 1;
    disc->next_group_id += 2;
    
    printf("分裂组 %d -> [%d, %d], 分裂点: %lld\n", 
           parent_group->group_id, left_group->group_id, right_group->group_id, split_point);
    
    return true;
}

// 边界调整函数（当达到最大组数时，调整边界以更好地平衡两端）
bool adjust_boundary(Discretizer* disc, TreeNode* edge_node, bool is_left_edge) {
    if (edge_node == NULL || edge_node->group == NULL || !edge_node->group->active) {
        return false;
    }
    
    Group* edge_group = edge_node->group;
    
    // 只有组内数据足够且违反条件时才调整
    if (edge_group->count < MIN_COUNT) {
        return false;
    }
    
    // 检查是否违反 max < 2 * min 条件（考虑 min=0 的特殊情况）
    bool violates = false;
    if (edge_group->min > 0) {
        violates = (edge_group->max >= 2 * edge_group->min);
    } else if (edge_group->min == 0) {
        violates = (edge_group->max >= 2);
    }
    
    if (!violates) {
        return false;  // 条件满足，无需调整
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
    
    printf("边界调整: %s边缘组 %d (min=%lld, max=%lld, count=%d)\n", 
           is_left_edge ? "左" : "右", edge_group->group_id, edge_group->min, edge_group->max, edge_group->count);
    
    // 改进版：基于实际数据分布计算分裂点
    long long new_split_point;
    long long edge_span = edge_group->max - edge_group->min;
    
    if (edge_group->count + sibling_group->count > 0 && edge_span > 1) {
        // 按数据量比例：edge占比 = edge->count / (edge->count + sibling->count)
        long long total_count = edge_group->count + sibling_group->count;
        long long edge_proportion = (edge_group->count * 100) / total_count;
        
        // 分裂点位置由数据占比决定
        new_split_point = edge_group->min + (edge_span * edge_proportion) / 100;
    } else {
        // 降级：使用中点
        new_split_point = (edge_group->min + edge_group->max) / 2;
    }
    
    // 确保分裂点在有效范围内
    if (new_split_point <= edge_group->min) new_split_point = edge_group->min + 1;
    if (new_split_point >= edge_group->max) new_split_point = edge_group->max - 1;
    
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
    if (disc == NULL || disc->root == NULL) return -1;

    // 第一个值到达时初始化
    if (!disc->initialized) {
        initialize_with_first_value(disc, value);
    }
    
    // 若值超出当前根节点范围，需要扩展根节点
    Group* root_group = disc->root->group;
    if (value < root_group->min) {
        root_group->min = value;
    }
    if (value > root_group->max) {
        root_group->max = value;
    }

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
            
            // 检查是否需要分裂
            if (disc->group_count < MAX_GROUPS) {
                if (need_split(group)) {
                    split_group(disc, current);
                }
            } else {
                // 达到最大组数，进行迭代边界微调以逐步优化分组
                int adjust_iterations = 0;
                const int MAX_ADJUST_ITERATIONS = 3;
                
                while (adjust_iterations < MAX_ADJUST_ITERATIONS) {
                    bool any_adjusted = false;
                    
                    TreeNode* leftmost = find_leftmost_leaf(disc->root);
                    TreeNode* rightmost = find_rightmost_leaf(disc->root);
                    
                    // 优先调整两端边缘
                    if (leftmost != NULL && leftmost->group != NULL && leftmost != rightmost) {
                        if (adjust_boundary(disc, leftmost, true)) {
                            any_adjusted = true;
                        }
                    }
                    
                    if (rightmost != NULL && rightmost->group != NULL && rightmost != leftmost) {
                        if (adjust_boundary(disc, rightmost, false)) {
                            any_adjusted = true;
                        }
                    }
                    
                    // 若本轮无调整，停止迭代
                    if (!any_adjusted) break;
                    
                    adjust_iterations++;
                }
                
                if (adjust_iterations > 0) {
                    printf("迭代调整完成: %d次迭代\n", adjust_iterations);
                }
            }
            
            return group_id;
        } else if (current->group == NULL) {
            // 内部节点（group为NULL）：根据分裂值决定方向
            if (value < current->split_value) {
                current = current->left;
            } else {
                current = current->right;
            }
        } else {
            // 不活跃的节点（已分裂），继续遍历子树
            if (value < current->split_value) {
                current = current->left;
            } else {
                current = current->right;
            }
        }
    }
    
    return -1;
}

// 初始化离散化器（创建临时根节点，等待第一个值到达后真正初始化）
void init_discretizer(Discretizer* disc, long long initial_min, long long initial_max) {
    disc->group_count = 1;
    disc->next_group_id = 1;
    disc->initialized = false;
    
    // 创建临时根组（范围待定）
    init_group(&disc->groups[0], 0);
    disc->groups[0].min = LLONG_MAX;
    disc->groups[0].max = LLONG_MIN;
    
    // 创建根节点
    disc->root = create_tree_node(0, &disc->groups[0]);
}

// 第一个值到达时，根据该值初始化根节点范围
void initialize_with_first_value(Discretizer* disc, long long first_value) {
    if (disc->initialized) return;
    
    // 设置初始范围为 [first_value, first_value]
    disc->groups[0].min = first_value;
    disc->groups[0].max = first_value;
    disc->initialized = true;
    
    printf("初始化根节点范围: [%lld, %lld]\n", first_value, first_value);
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
