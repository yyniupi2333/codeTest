#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/discretizer.h"

int main() {
    Discretizer disc;
    
    // 初始化离散化器（临时状态，等待第一个值到达）
    init_discretizer(&disc, 0, LLONG_MAX);
    
    // 原始测试数据（Fibonacci序列）
    long long fib_data[] = {
        1, 2, 3, 5, 8, 13, 21, 34, 55, 89,
        144, 233, 377, 610, 987, 1597, 2584, 4181,
        6765, 10946, 17711, 28657, 46368, 75025,
        121393, 196418, 317811, 514229, 832040
    };
    
    // 生成额外的随机测试数据
    srand((unsigned)time(NULL));
    long long test_data[530];  // 29个Fibonacci + 501个随机数
    
    // 复制Fibonacci数据
    int fib_count = sizeof(fib_data) / sizeof(fib_data[0]);
    for (int i = 0; i < fib_count; i++) {
        test_data[i] = fib_data[i];
    }
    
    // 生成随机数据
    for (int i = fib_count; i < 530; i++) {
        test_data[i] = (long long)rand() % 1000000 + 1;
    }
    
    int data_count = 530;
    
    printf("开始整数离散化处理...\n");
    printf("总数据量: %d, 目标组数: %d, 每组需满足 max < 2 * min\n", data_count, MAX_GROUPS);
    printf("初始范围: [0, %lld]\n\n", LLONG_MAX);
    
    // 处理数据流（处理过程中不打印每条记录，以加快处理）
    int max_groups_reached = 0;
    for (int i = 0; i < data_count; i++) {
        if (disc.group_count >= MAX_GROUPS && !max_groups_reached) {
            printf("✓ 已达到最大组数 %d，启用边界调整模式\n\n", MAX_GROUPS);
            max_groups_reached = 1;
        }
        
        query_and_update(&disc, test_data[i]);
        
        // 每处理100条数据打印进度
        if ((i + 1) % 100 == 0) {
            printf("已处理 %d 条数据，当前组数: %d\n", i + 1, disc.group_count);
        }
    }
    
    printf("\n✓ 数据处理完成，共处理 %d 条数据\n", data_count);
    
    // 打印最终结果
    print_groups(&disc);
    
    // 测试查询功能
    printf("\n纯查询测试（不更新统计）:\n");
    long long test_values[] = {10, 100, 1000, 10000, 50000, 100000, 500000};
    for (int i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        int group_id = find_group_id(&disc, test_values[i]);
        printf("值 %8lld 属于组 %d\n", test_values[i], group_id);
    }
    
    // 清理资源
    free_tree(disc.root);
    
    return 0;
}
