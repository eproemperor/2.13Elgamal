#include <rclcpp/rclcpp.hpp>
#include <rm_server/msg/get_el_gamal_params.hpp>
#include <std_msgs/msg/int64.hpp>
#include <random>
#include <string>
#include <cstdio>
#include <memory>

class ElgamalClient : public rclcpp::Node
{
public:
    ElgamalClient() : Node("elgamal_client")
    {
        sub_ = this->create_subscription<rm_server::msg::GetElGamalParams>(
            "/elgamal_params", 10,
            std::bind(&ElgamalClient::params_callback, this, std::placeholders::_1));
        
        pub_ = this->create_publisher<std_msgs::msg::Int64>("/elgamal_result", 10);
        
        RCLCPP_INFO(this->get_logger(), "=================================");
        RCLCPP_INFO(this->get_logger(), "ElGamal客户端已启动（系统命令版）");
        RCLCPP_INFO(this->get_logger(), "=================================");
    }

private:
    uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod)
    {
        uint64_t result = 1;
        base %= mod;
        while (exp > 0) {
            if (exp & 1) result = (result * base) % mod;
            base = (base * base) % mod;
            exp >>= 1;
        }
        return result;
    }

    void params_callback(const rm_server::msg::GetElGamalParams::SharedPtr msg)
    {
        p_ = msg->p;
        a_ = msg->a;
        
        RCLCPP_INFO(this->get_logger(), "收到参数: p=%lu, a=%lu", p_, a_);
        
        // 生成私钥
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(2, p_ - 2);
        n_ = dis(gen);
        
        // 公钥 b = a^n mod p
        b_ = mod_pow(a_, n_, p_);
        
        RCLCPP_INFO(this->get_logger(), "私钥 n=%lu, 公钥 b=%lu", n_, b_);
        
        call_service_by_system();
    }
    
    void call_service_by_system()
    {
        RCLCPP_INFO(this->get_logger(), "正在调用服务 /elgamal_service ...");
        
        // 构造命令 - 完全模拟手动调用
        std::string cmd = "ros2 service call /elgamal_service rm_server/srv/ElGamalEncrypt "
                          "\"{public_key: " + std::to_string(b_) + "}\" 2>&1";
        
        RCLCPP_DEBUG(this->get_logger(), "执行: %s", cmd.c_str());
        
        // 执行命令并捕获输出
        char buffer[1024];
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        
        if (!pipe) {
            RCLCPP_ERROR(this->get_logger(), "命令执行失败");
            return;
        }
        
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        int ret = pclose(pipe);
        
        RCLCPP_INFO(this->get_logger(), "服务返回:");
        RCLCPP_INFO(this->get_logger(), "%s", result.c_str());
        
        // 解析y1,y2
        parse_response(result);
    }
    
    void parse_response(const std::string& response)
    {
        // 查找y1和y2
        size_t y1_pos = response.find("y1=");
        size_t y2_pos = response.find("y2=");
        
        if (y1_pos != std::string::npos && y2_pos != std::string::npos) {
            // 提取y1
            std::string y1_str = response.substr(y1_pos + 3);
            y1_ = std::stoull(y1_str);
            
            // 提取y2
            std::string y2_str = response.substr(y2_pos + 3);
            y2_ = std::stoull(y2_str);
            
            RCLCPP_INFO(this->get_logger(), "收到密文: y1=%lu, y2=%lu", y1_, y2_);
            
            // 解密
            decrypt();
        } else {
            RCLCPP_ERROR(this->get_logger(), "无法解析服务响应");
        }
    }
    
    void decrypt()
    {
        // 任务书公式: x = y2 * (y1^n)^(p-2) mod p
        uint64_t s = mod_pow(y1_, n_, p_);        // s = y1^n mod p
        uint64_t s_inv = mod_pow(s, p_ - 2, p_);  // s^(p-2) mod p
        uint64_t x = (y2_ * s_inv) % p_;          // x = y2 * s^(-1) mod p
        
        RCLCPP_INFO(this->get_logger(), "解密得到明文: %lu", x);
        
        // 发布结果
        std_msgs::msg::Int64 result;
        result.data = x;
        pub_->publish(result);
        
        RCLCPP_INFO(this->get_logger(), "已发布解密结果到 /elgamal_result");
        RCLCPP_INFO(this->get_logger(), "等待下一轮参数...\n");
    }

    rclcpp::Subscription<rm_server::msg::GetElGamalParams>::SharedPtr sub_;
    rclcpp::Publisher<std_msgs::msg::Int64>::SharedPtr pub_;
    
    uint64_t p_, a_;     // 服务端参数
    uint64_t n_, b_;     // 客户端密钥对
    uint64_t y1_, y2_;   // 密文
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ElgamalClient>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
