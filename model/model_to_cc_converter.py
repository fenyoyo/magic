"""
模型到C++头文件转换器
此脚本将TensorFlow Lite模型转换为C++头文件(.h)，以便在嵌入式系统中使用
"""

import os
import sys
import tensorflow as tf
import numpy as np


def convert_tflite_to_cc_array(tflite_model_path, output_header_path, array_name=None):
    """
    将TFLite模型转换为C++数组格式
    
    Args:
        tflite_model_path: 输入的TFLite模型路径
        output_header_path: 输出的C++头文件路径
        array_name: C++数组名称，默认为基于文件名生成
    """
    # 读取TFLite模型
    with open(tflite_model_path, 'rb') as f:
        tflite_model_data = f.read()
    
    # 如果没有指定数组名，则基于文件名生成
    if array_name is None:
        base_name = os.path.splitext(os.path.basename(tflite_model_path))[0]
        array_name = f"{base_name}_tflite"
    
    # 生成C++头文件内容
    header_content = f'''// Auto-generated C++ header file for TFLite model
// Source: {tflite_model_path}

#ifndef {array_name.upper()}_H
#define {array_name.upper()}_H

#include <cstdint>

// TFLite模型数据数组
alignas(16) static const uint8_t {array_name}[] = {{
'''
    
    # 将模型数据转换为C++数组格式
    bytes_per_line = 12  # 每行显示的字节数
    for i, byte in enumerate(tflite_model_data):
        if i % bytes_per_line == 0:
            if i != 0:
                header_content += '\n'
            header_content += '  '
        header_content += f'0x{byte:02x}'
        if i != len(tflite_model_data) - 1:
            header_content += ', '
    
    header_content += f'''
}};
static const unsigned int {array_name}_len = {len(tflite_model_data)};

#endif // {array_name.upper()}_H
'''
    
    # 写入头文件
    with open(output_header_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"成功将模型 '{tflite_model_path}' 转换为 C++ 头文件 '{output_header_path}'")
    print(f"数组名称: {array_name}")
    print(f"模型大小: {len(tflite_model_data)} 字节")


def convert_h5_to_tflite_and_cc(h5_model_path, output_header_path=None, array_name=None):
    """
    将H5模型转换为TFLite格式，然后再转换为C++数组
    
    Args:
        h5_model_path: 输入的H5模型路径
        output_header_path: 输出的C++头文件路径，如果未指定则自动生成
        array_name: C++数组名称，默认为基于文件名生成
    """
    # 加载H5模型
    print(f"加载H5模型: {h5_model_path}")
    model = tf.keras.models.load_model(h5_model_path)
    
    # 转换为TFLite
    print("转换为TFLite模型...")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # 设置优化选项
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    # 尝试量化为INT8（适用于TinyML）
    def representative_dataset():
        # 创建一个简单的示例数据集用于量化
        for _ in range(100):
            # 根据您的模型输入形状调整这里的示例数据
            # 这里假设输入形状为 (None, 250, 6)，根据您的模型调整
            yield [np.random.random((1,) + model.input_shape[1:]).astype(np.float32)]

    try:
        # 对于包含LSTM的模型，可能需要特殊处理
        converter.representative_dataset = representative_dataset
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8
        
        # 对于某些复杂操作，启用Select TF ops
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS, tf.lite.OpsSet.SELECT_TF_OPS]
        converter._experimental_lower_tensor_list_ops = False
        
        tflite_model = converter.convert()
    except Exception as e:
        print(f"量化转换失败: {e}")
        print("尝试不使用量化的转换...")
        # 如果量化失败，尝试基本转换
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        # 最基础的转换
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS]
        try:
            tflite_model = converter.convert()
        except Exception as e2:
            print(f"基础转换也失败: {e2}")
            print("尝试使用Select TF ops而不降低tensor list ops...")
            # 再次尝试使用Select TF ops但不降低tensor list ops
            converter = tf.lite.TFLiteConverter.from_keras_model(model)
            converter.optimizations = [tf.lite.Optimize.DEFAULT]
            converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS, tf.lite.OpsSet.SELECT_TF_OPS]
            converter._experimental_lower_tensor_list_ops = False
            tflite_model = converter.convert()
    
    # 生成TFLite文件路径
    if output_header_path is None:
        base_name = os.path.splitext(os.path.basename(h5_model_path))[0]
        tflite_path = f"{base_name}.tflite"
        output_header_path = f"{base_name}_model_data.h"
    else:
        base_name = os.path.splitext(os.path.basename(output_header_path))[0]
        tflite_path = f"{base_name}.tflite"
    
    # 保存TFLite模型
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    
    print(f"TFLite模型已保存为: {tflite_path}")
    
    # 将TFLite模型转换为C++数组
    if array_name is None:
        array_name = f"{base_name.replace('-', '_')}_model"
    
    convert_tflite_to_cc_array(tflite_path, output_header_path, array_name)
    
    return tflite_path, output_header_path


def main():
    if len(sys.argv) < 2:
        print("用法:")
        print("  python model_to_cc_converter.py <model_path> [output_header_path] [array_name]")
        print("")
        print("示例:")
        print("  python model_to_cc_converter.py gyro_shape_classifier_enhanced.tflite")
        print("  python model_to_cc_converter.py gyro_shape_classifier_enhanced.tflite gyro_model_data.h gyro_model")
        print("  python model_to_cc_converter.py gyro_shape_classifier_enhanced.h5")
        return
    
    input_path = sys.argv[1]
    
    # 检查输入文件是否存在
    if not os.path.exists(input_path):
        print(f"错误: 输入文件不存在 - {input_path}")
        return
    
    # 根据文件扩展名决定处理方式
    if input_path.lower().endswith('.h5'):
        # H5模型文件，需要先转换为TFLite再转换为C++
        output_header_path = sys.argv[2] if len(sys.argv) > 2 else None
        array_name = sys.argv[3] if len(sys.argv) > 3 else None
        
        print(f"检测到H5模型，正在转换: {input_path}")
        convert_h5_to_tflite_and_cc(input_path, output_header_path, array_name)
        
    elif input_path.lower().endswith('.tflite'):
        # TFLite模型文件，直接转换为C++
        output_header_path = sys.argv[2] if len(sys.argv) > 2 else None
        array_name = sys.argv[3] if len(sys.argv) > 3 else None
        
        if output_header_path is None:
            base_name = os.path.splitext(os.path.basename(input_path))[0]
            output_header_path = f"{base_name}_model_data.h"
        
        if array_name is None:
            array_name = f"{os.path.splitext(os.path.basename(input_path))[0].replace('-', '_')}_model"
        
        print(f"检测到TFLite模型，正在转换: {input_path}")
        convert_tflite_to_cc_array(input_path, output_header_path, array_name)
        
    else:
        print(f"错误: 不支持的文件格式 - {input_path}")
        print("支持的格式: .h5, .tflite")


if __name__ == "__main__":
    main()