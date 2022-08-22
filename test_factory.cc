#include "design_pattern.h"

#include <iostream>

class Car
{
  public:
    enum Type_E
    {
        Fuel,
        Fuel_Electric,
        Electric
    };

    struct Info
    {
        std::string name;             // name of the object
        Type_E type;                  // 驱动类型
        int price_rmb;                // 售价（元）
        int warranty_month;           // 保修期（月）
        int power_kW;                 // 功率 kW
        int mileage_km;               // 续航里程（km）
        float max_torque_Nm;          // 最大扭矩（N·m）
        float max_speed_kmph;         // 最高时速(km/h)
        float acceleration_100km_sec; //百公里加速（sec）
        union {
            float fuel_economy_Lp100km;  // 油耗（L/100km）
            float elec_enconomy_kW100km; // 电耗（kW/100km）
        };
    };

    virtual const Info &Info() const
    {
        return info_;
    };

    virtual void Crash() = 0;

    virtual ~Car()
    {
    }

  protected:
    struct Info info_;
};

class TESLA : public Car
{
  public:
    TESLA()
    {
        info_.name = "Model X 2021款 三电机全轮驱动 Plaid版";
        info_.type = Electric;
        info_.price_rmb = 360000;
        info_.warranty_month = 48;
        info_.power_kW = 750;
        info_.mileage_km = 536;
        info_.max_torque_Nm = 440;
        info_.max_speed_kmph = 262;
        info_.acceleration_100km_sec = 2.6f;
        info_.elec_enconomy_kW100km = 13.2f;
    }

    void Crash() override
    {
        std::cout << info_.name << "（" << info_.max_speed_kmph << "km/h）>>>>>> 老子要燃起来！" << std::endl;
    }
};

class BYD : public Car
{
  public:
    BYD(const std::string &attack_result)
    {
        info_.name = "唐新能源 2022款 DM-p 215KM 四驱旗舰型";
        info_.type = Electric;
        info_.price_rmb = 339800;
        info_.warranty_month = 72;
        info_.power_kW = 360;
        info_.mileage_km = 635;
        info_.max_torque_Nm = 675;
        info_.max_speed_kmph = 180;
        info_.acceleration_100km_sec = 4.4f;
        info_.elec_enconomy_kW100km = 17.6f;

        crash_result_ = attack_result;
    }

    void Crash() override
    {
        std::cout << info_.name << "（" << info_.max_speed_kmph << "km/h）>>>>>> " << crash_result_ << std::endl;
    }

  private:
    std::string crash_result_;
};

class Oil
{
  public:
    virtual ~Oil(){};
    virtual std::string name() const = 0;
};

class Gasoline : public Oil
{
  public:
    Gasoline(int type) : type_num_(type){};

    std::string name() const
    {
        return std::to_string(type_num_) + "号汽油";
    }

  private:
    int type_num_;
};

class Diesel : public Oil
{
  public:
    Diesel(bool is_weight) : is_weight_(is_weight){};
    std::string name() const
    {
        return is_weight_ ? "重柴油" : "轻柴油";
    }

  private:
    bool is_weight_;
};

int main()
{
    try
    {
        // 偏特化版本类实例
        Factory<Car>::Instance().RegistProduct<TESLA>("特化TESLA");
        Factory<Car>::Instance().RegistProduct<BYD, const std::string &>("特化BYD");

        Factory<Oil>::Instance().RegistProduct<Gasoline, int>("特化Gasoline");
        Factory<Oil>::Instance().RegistProduct<Diesel, bool>("特化Diesel");
        
        // 通用模板类实例
        Factory<Car, std::string, const std::string &>::Instance().RegistProduct<BYD>("BYD");

        Factory<Oil, std::string, int>::Instance().RegistProduct<Gasoline>("Gasoline");

        Factory<Oil, std::string, bool>::Instance().RegistProduct<Diesel>("Diesel");


        // 前面总共创建了5个工厂实例
        std::cout << Factory<Car, std::string, const std::string &>::Instance().AllRegistedProducts() << std::endl;
        std::cout << Factory<Car>::Instance().AllRegistedProducts() << std::endl;
        std::cout << Factory<Car, int>::Instance().AllRegistedProducts() << std::endl;
        std::cout << Factory<Oil>::Instance().AllRegistedProducts() << std::endl;
        std::cout << Factory<Oil, std::string, int>::Instance().AllRegistedProducts() << std::endl;
        std::cout << Factory<Oil, std::string, bool>::Instance().AllRegistedProducts() << std::endl;

        const std::string str = "当仁不让！";
        
        {
            auto oil = Factory<Oil>::Instance().NewProduct("特化Gasoline", 92);
            std::cout << oil->name() << std::endl;
        }
        
        {
            auto oil = Factory<Oil, std::string, int>::Instance().NewProduct("Gasoline", 93);
            std::cout << oil->name() << std::endl;
        }
        
        {
            auto oil = Factory<Oil>::Instance().NewProduct("特化Diesel", true);
            std::cout << oil->name() << std::endl;
        }
        
        {
            auto oil = Factory<Oil, std::string, bool>::Instance().NewProduct("Diesel", false);
            std::cout << oil->name() << std::endl;
        }

        //----------------------------------------------------------------//

        {
            auto BYD = Factory<Car>::Instance().NewProduct("特化BYD", str);
            BYD->Crash();
        }
        
        {
            auto TESLA = Factory<Car>::Instance().NewProduct("特化TESLA");
            TESLA->Crash();
        }
        
        {
            auto BYD = Factory<Car, std::string, const std::string &>::Instance().NewProduct("BYD", str);
            BYD->Crash();
        }
        
        {
            auto BYD = Factory<Car>::Instance().NewProduct("特化BYD", str);
            BYD->Crash();
        }
        
        // error const type.
        {
            std::string nonconst_str_lval(str);
            auto BYD = Factory<Car>::Instance().NewProduct("特化BYD", nonconst_str_lval);
            BYD->Crash();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        // return 1;
    }

    return 0;
}
