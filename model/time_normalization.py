import numpy as np
from scipy.interpolate import interp1d


def time_normalize_sequence(sequence, target_length):
    """
    使用线性插值对时间序列进行归一化
    
    参数:
    sequence: 输入的时间序列，形状为 (sequence_length, features)
    target_length: 目标长度
    
    返回:
    归一化后的时间序列，形状为 (target_length, features)
    """
    if len(sequence) == target_length:
        return sequence
    elif len(sequence) < 2:
        # 如果序列太短，无法插值，则使用填充
        if len(sequence) == 0:
            return np.zeros((target_length, sequence.shape[1]))
        else:
            # 复制唯一的数据点直到达到目标长度
            repeated_sequence = np.tile(sequence[0], (target_length, 1))
            return repeated_sequence
    else:
        # 创建原始序列的时间轴
        original_indices = np.linspace(0, 1, len(sequence))
        target_indices = np.linspace(0, 1, target_length)
        
        # 对每个特征维度分别进行线性插值
        normalized_sequence = np.zeros((target_length, sequence.shape[1]))
        
        for feature_idx in range(sequence.shape[1]):
            interpolator = interp1d(original_indices, sequence[:, feature_idx], 
                                   kind='linear', fill_value='extrapolate')
            normalized_sequence[:, feature_idx] = interpolator(target_indices)
        
        return normalized_sequence


def time_normalize_batch(sequences, target_length):
    """
    批量对多个时间序列进行时间归一化
    
    参数:
    sequences: 输入的时间序列批次，形状为 (batch_size, sequence_length, features)
    target_length: 目标长度
    
    返回:
    归一化后的序列批次，形状为 (batch_size, target_length, features)
    """
    normalized_sequences = np.zeros((len(sequences), target_length, sequences.shape[2]))
    
    for i, seq in enumerate(sequences):
        normalized_sequences[i] = time_normalize_sequence(seq, target_length)
    
    return normalized_sequences