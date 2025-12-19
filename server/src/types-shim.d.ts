declare module "cors";
declare module "dgram";
declare module "dotenv";
declare module "mysql2/promise" {
  export type Pool = any;
  export type PoolConnection = any;
  export function createPool(config: any): any;
}
declare module "express" {
  export type Request = any;
  export type Response = any;
  export type NextFunction = any;
  const e: any;
  export default e;
}
declare module "jsonwebtoken";
