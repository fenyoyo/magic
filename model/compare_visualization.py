import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from time_normalization import time_normalize_sequence

def compare_original_and_normalized_eng(file_path, target_length=100):
    """
    Compare original data and time-normalized data (English labels version)
    """
    # 读取原始数据
    df = pd.read_csv(file_path)
    original_data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values
    original_length = len(original_data)
    
    print(f"Original data length: {original_length}")
    print(f"Target length: {target_length}")

    # Apply time normalization
    normalized_data = time_normalize_sequence(original_data, target_length)
    
    # Create comparison plots
    fig, axes = plt.subplots(3, 2, figsize=(16, 12))
    
    # Accelerometer data comparison
    axes[0, 0].plot(range(original_length), original_data[:, 0], label='ax', color='red', alpha=0.7)
    axes[0, 0].plot(range(original_length), original_data[:, 1], label='ay', color='green', alpha=0.7)
    axes[0, 0].plot(range(original_length), original_data[:, 2], label='az', color='blue', alpha=0.7)
    axes[0, 0].set_title(f'Original Accelerometer Data (Length: {original_length})')
    axes[0, 0].set_xlabel('Time Index')
    axes[0, 0].set_ylabel('Acceleration Value')
    axes[0, 0].legend()
    axes[0, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[0, 1].plot(range(target_length), normalized_data[:, 0], label='ax', color='red', alpha=0.7)
    axes[0, 1].plot(range(target_length), normalized_data[:, 1], label='ay', color='green', alpha=0.7)
    axes[0, 1].plot(range(target_length), normalized_data[:, 2], label='az', color='blue', alpha=0.7)
    axes[0, 1].set_title(f'Time-Normalized Accelerometer Data (Length: {target_length})')
    axes[0, 1].set_xlabel('Time Index')
    axes[0, 1].set_ylabel('Acceleration Value')
    axes[0, 1].legend()
    axes[0, 1].grid(True, linestyle='--', alpha=0.3)
    
    # Gyroscope data comparison
    axes[1, 0].plot(range(original_length), original_data[:, 3], label='gx', color='orange', alpha=0.7)
    axes[1, 0].plot(range(original_length), original_data[:, 4], label='gy', color='purple', alpha=0.7)
    axes[1, 0].plot(range(original_length), original_data[:, 5], label='gz', color='brown', alpha=0.7)
    axes[1, 0].set_title(f'Original Gyroscope Data (Length: {original_length})')
    axes[1, 0].set_xlabel('Time Index')
    axes[1, 0].set_ylabel('Angular Velocity Value')
    axes[1, 0].legend()
    axes[1, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[1, 1].plot(range(target_length), normalized_data[:, 3], label='gx', color='orange', alpha=0.7)
    axes[1, 1].plot(range(target_length), normalized_data[:, 4], label='gy', color='purple', alpha=0.7)
    axes[1, 1].plot(range(target_length), normalized_data[:, 5], label='gz', color='brown', alpha=0.7)
    axes[1, 1].set_title(f'Time-Normalized Gyroscope Data (Length: {target_length})')
    axes[1, 1].set_xlabel('Time Index')
    axes[1, 1].set_ylabel('Angular Velocity Value')
    axes[1, 1].legend()
    axes[1, 1].grid(True, linestyle='--', alpha=0.3)
    
    # Single feature comparison (ax as example)
    axes[2, 0].plot(range(original_length), original_data[:, 0], label='Original ax data', color='red', alpha=0.7)
    axes[2, 0].set_title(f'Original ax Data (Length: {original_length})')
    axes[2, 0].set_xlabel('Time Index')
    axes[2, 0].set_ylabel('ax Value')
    axes[2, 0].legend()
    axes[2, 0].grid(True, linestyle='--', alpha=0.3)
    
    axes[2, 1].plot(range(target_length), normalized_data[:, 0], label='Normalized ax data', color='red', alpha=0.7)
    axes[2, 1].set_title(f'Normalized ax Data (Length: {target_length})')
    axes[2, 1].set_xlabel('Time Index')
    axes[2, 1].set_ylabel('ax Value')
    axes[2, 1].legend()
    axes[2, 1].grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    # Print statistics
    print("\nData Statistics Comparison:")
    print(f"Original data - Mean: ax={np.mean(original_data[:, 0]):.3f}, ay={np.mean(original_data[:, 1]):.3f}, az={np.mean(original_data[:, 2]):.3f}")
    print(f"Normalized data - Mean: ax={np.mean(normalized_data[:, 0]):.3f}, ay={np.mean(normalized_data[:, 1]):.3f}, az={np.mean(normalized_data[:, 2]):.3f}")
    print(f"Original data - Std: ax={np.std(original_data[:, 0]):.3f}, ay={np.std(original_data[:, 1]):.3f}, az={np.std(original_data[:, 2]):.3f}")
    print(f"Normalized data - Std: ax={np.std(normalized_data[:, 0]):.3f}, ay={np.std(normalized_data[:, 1]):.3f}, az={np.std(normalized_data[:, 2]):.3f}")


if __name__ == "__main__":
    # Use provided data file path
    file_path = 'test/none_001.csv'
    print("Comparing original and normalized data:")
    compare_original_and_normalized_eng(file_path)